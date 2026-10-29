/**
 * @file gateway_server.cpp
 * @brief HTTP 网关实现（M7 第一步：注册/登录/用户信息/历史消息）。
 */

#include "gateway_server.hpp"

#include <chrono>

#include <gflags/gflags.h>

#include "base.pb.h"
#include "etcd.hpp"
#include "file.pb.h"
#include "friend.pb.h"
#include "logger.hpp"
#include "message.pb.h"
#include "transmit.pb.h"
#include "user.pb.h"

DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/gateway", "发布模式日志目录");
DEFINE_int32(log_level, 0, "日志等级");
DEFINE_int32(http_port, 9000, "HTTP 监听端口");
DEFINE_int32(ws_port, 9001, "WebSocket 监听端口");
DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd 地址");
DEFINE_string(service_base, "/im/service", "服务注册根目录");
DEFINE_string(user_service, "/im/service/user", "用户服务名");
DEFINE_string(message_service, "/im/service/message", "消息服务名");
DEFINE_string(friend_service, "/im/service/friend", "好友服务名");
DEFINE_string(transmit_service, "/im/service/transmit", "消息转发服务名");
DEFINE_string(file_service, "/im/service/file", "文件服务名");
DEFINE_string(redis_host, "127.0.0.1", "Redis 地址");
DEFINE_int32(redis_port, 6379, "Redis 端口");
DEFINE_int32(redis_db, 0, "Redis 库号");

namespace im {

/**
 * @brief 实现构造并注册路由。
 */
GatewayServer::GatewayServer(int port,
                             int ws_port,
                             std::shared_ptr<sw::redis::Redis> redis,
                             ServiceManager::Ptr channels,
                             std::string user_service,
                             std::string friend_service,
                             std::string transmit_service,
                             std::string message_service,
                             std::string file_service)
    : sessions_(std::make_shared<SessionStore>(std::move(redis),
                                               std::chrono::seconds(7 * 24 * 3600))),
      channels_(std::move(channels)),
      user_service_(std::move(user_service)),
      friend_service_(std::move(friend_service)),
      transmit_service_(std::move(transmit_service)),
      message_service_(std::move(message_service)),
      file_service_(std::move(file_service)) {
    (void)port;

    http_.Post("/api/user/register",
               [this](const httplib::Request& req, httplib::Response& res) {
                   handle<false, UserRegisterReq, UserRegisterRsp>(
                       user_service_, req, res,
                       [](brpc::Channel* ch, UserRegisterReq& rq, UserRegisterRsp& rs,
                          brpc::Controller& c) {
                           UserService_Stub stub(ch);
                           stub.UserRegister(&c, &rq, &rs, nullptr);
                       });
               });

    http_.Post("/api/user/login",
               [this](const httplib::Request& req, httplib::Response& res) {
                   handle<false, UserLoginReq, UserLoginRsp>(
                       user_service_, req, res,
                       [](brpc::Channel* ch, UserLoginReq& rq, UserLoginRsp& rs,
                          brpc::Controller& c) {
                           UserService_Stub stub(ch);
                           stub.UserLogin(&c, &rq, &rs, nullptr);
                       });
               });

    http_.Post("/api/user/info",
               [this](const httplib::Request& req, httplib::Response& res) {
                   handle<true, GetUserInfoReq, GetUserInfoRsp>(
                       user_service_, req, res,
                       [](brpc::Channel* ch, GetUserInfoReq& rq, GetUserInfoRsp& rs,
                          brpc::Controller& c) {
                           UserService_Stub stub(ch);
                           stub.GetUserInfo(&c, &rq, &rs, nullptr);
                       });
               });

    http_.Post("/api/message/history",
               [this](const httplib::Request& req, httplib::Response& res) {
                   handle<true, GetHistoryMsgReq, GetHistoryMsgRsp>(
                       message_service_, req, res,
                       [](brpc::Channel* ch, GetHistoryMsgReq& rq, GetHistoryMsgRsp& rs,
                          brpc::Controller& c) {
                           MessageService_Stub stub(ch);
                           stub.GetHistory(&c, &rq, &rs, nullptr);
                       });
               });

#define IM_ROUTE(path, ReqT, RspT, StubT, method, service, need_auth)                    \
    http_.Post(path, [this](const httplib::Request& req, httplib::Response& res) {       \
        handle<need_auth, ReqT, RspT>(service, req, res,                                 \
            [](brpc::Channel* ch, ReqT& rq, RspT& rs, brpc::Controller& c) {             \
                StubT stub(ch);                                                          \
                stub.method(&c, &rq, &rs, nullptr);                                      \
            });                                                                          \
    })

    IM_ROUTE("/api/friend/list", GetFriendListReq, GetFriendListRsp,
             FriendService_Stub, GetFriendList, friend_service_, true);
    IM_ROUTE("/api/friend/add", FriendAddReq, FriendAddRsp, FriendService_Stub,
             FriendAdd, friend_service_, true);
    IM_ROUTE("/api/friend/process", FriendAddProcessReq, FriendAddProcessRsp,
             FriendService_Stub, FriendAddProcess, friend_service_, true);
    IM_ROUTE("/api/friend/pending", GetPendingFriendEventListReq,
             GetPendingFriendEventListRsp, FriendService_Stub,
             GetPendingFriendEventList, friend_service_, true);
    IM_ROUTE("/api/friend/remove", FriendRemoveReq, FriendRemoveRsp, FriendService_Stub,
             FriendRemove, friend_service_, true);
    IM_ROUTE("/api/friend/search", FriendSearchReq, FriendSearchRsp, FriendService_Stub,
             FriendSearch, friend_service_, true);
    IM_ROUTE("/api/session/list", GetChatSessionListReq, GetChatSessionListRsp,
             FriendService_Stub, GetChatSessionList, friend_service_, true);
    IM_ROUTE("/api/session/create", ChatSessionCreateReq, ChatSessionCreateRsp,
             FriendService_Stub, ChatSessionCreate, friend_service_, true);
    IM_ROUTE("/api/session/members", GetChatSessionMemberReq, GetChatSessionMemberRsp,
             FriendService_Stub, GetChatSessionMember, friend_service_, true);

    IM_ROUTE("/api/message/recent", GetRecentMsgReq, GetRecentMsgRsp,
             MessageService_Stub, GetRecent, message_service_, true);
    IM_ROUTE("/api/message/search", MsgSearchReq, MsgSearchRsp,
             MessageService_Stub, Search, message_service_, true);

    IM_ROUTE("/api/file/put", PutFileRequest, PutFileResponse, FileService_Stub,
             PutFile, file_service_, false);
    IM_ROUTE("/api/file/get", GetFileRequest, GetFileResponse, FileService_Stub,
             GetFile, file_service_, false);

#undef IM_ROUTE

    ws_.set_access_channels(websocketpp::log::alevel::none);
    ws_.init_asio();
    ws_.set_reuse_addr(true);
    ws_.set_message_handler([this](websocketpp::connection_hdl hdl, WsServer::message_ptr msg) {
        onWsMessage(hdl, msg);
    });
    ws_.set_close_handler([this](websocketpp::connection_hdl hdl) { onWsClose(hdl); });
    ws_.listen(ws_port_);
    ws_.start_accept();

    http_.Post("/api/message/send",
               [this](const httplib::Request& req, httplib::Response& res) {
                   SendMessageReq request;
                   SendMessageRsp response;
                   auto respond = [&]() {
                       res.set_content(response.SerializeAsString(),
                                       "application/x-protobuf");
                   };
                   if (!request.ParseFromString(req.body)) {
                       response.set_success(false);
                       response.set_errmsg("请求正文解析失败");
                       return respond();
                   }
                   response.set_request_id(request.request_id());
                   auto uid = sessions_->userId(request.session_id());
                   if (!uid) {
                       response.set_success(false);
                       response.set_errmsg("未登录或会话已过期");
                       return respond();
                   }
                   request.set_user_id(*uid);
                   auto channel = channels_->choose(transmit_service_);
                   if (!channel) {
                       response.set_success(false);
                       response.set_errmsg("服务不可用");
                       return respond();
                   }
                   brpc::Controller cntl;
                   cntl.set_timeout_ms(5000);
                   TransmitService_Stub stub(channel.get());
                   stub.SendMessage(&cntl, &request, &response, nullptr);
                   if (cntl.Failed()) {
                       response.set_success(false);
                       response.set_errmsg("调用服务失败");
                   }
                   respond();
                   if (response.success()) {
                       pushMessage(response);
                   }
               });

    http_.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        res.status = 404;
        res.set_content("not found", "text/plain");
    });
    IM_INFO("gateway routes registered");
}

/**
 * @brief 实现通用处理。
 */
template <bool NeedAuth, typename ReqT, typename RspT, typename Call>
void GatewayServer::handle(const std::string& service,
                           const httplib::Request& req, httplib::Response& res,
                           Call call) {
    ReqT request;
    RspT response;
    auto respond = [&]() {
        res.set_content(response.SerializeAsString(), "application/x-protobuf");
    };

    if (!request.ParseFromString(req.body)) {
        response.set_success(false);
        response.set_errmsg("请求正文解析失败");
        return respond();
    }
    if (!request.request_id().empty()) {
        response.set_request_id(request.request_id());
    }

    if constexpr (NeedAuth) {
        auto uid = sessions_->userId(request.session_id());
        if (!uid) {
            response.set_success(false);
            response.set_errmsg("未登录或会话已过期");
            return respond();
        }
        request.set_user_id(*uid);
    }

    auto channel = channels_->choose(service);
    if (!channel) {
        response.set_success(false);
        response.set_errmsg("服务不可用");
        return respond();
    }

    brpc::Controller cntl;
    cntl.set_timeout_ms(5000);
    call(channel.get(), request, response, cntl);
    if (cntl.Failed()) {
        response.set_success(false);
        response.set_errmsg("调用服务失败");
    }
    respond();
}

/**
 * @brief 实现启动。
 */
void GatewayServer::start() {
    http_thread_ = std::thread([this]() {
        IM_INFO("gateway http listening on 0.0.0.0:{}", FLAGS_http_port);
        http_.listen("0.0.0.0", FLAGS_http_port);
    });
    IM_INFO("gateway websocket listening on 0.0.0.0:{}", ws_port_);
    ws_.run();
}

void GatewayServer::onWsMessage(websocketpp::connection_hdl hdl,
                                WsServer::message_ptr msg) {
    auto conn = ws_.get_con_from_hdl(hdl);
    ClientAuthenticationReq request;
    if (!request.ParseFromString(msg->get_payload())) {
        ws_.close(hdl, websocketpp::close::status::unsupported_data, "bad auth");
        return;
    }
    auto uid = sessions_->userId(request.session_id());
    if (!uid) {
        ws_.close(hdl, websocketpp::close::status::unsupported_data, "invalid session");
        return;
    }
    std::lock_guard<std::mutex> lock(ws_mutex_);
    uid_conns_[*uid] = conn;
    conn_uids_[conn] = *uid;
    IM_INFO("ws auth ok uid={}", *uid);
}

void GatewayServer::onWsClose(websocketpp::connection_hdl hdl) {
    auto conn = ws_.get_con_from_hdl(hdl);
    std::lock_guard<std::mutex> lock(ws_mutex_);
    auto it = conn_uids_.find(conn);
    if (it != conn_uids_.end()) {
        uid_conns_.erase(it->second);
        conn_uids_.erase(it);
    }
}

void GatewayServer::pushMessage(const SendMessageRsp& response) {
    NotifyMessage notify;
    notify.set_type(1);
    notify.mutable_message()->CopyFrom(response.message());
    const std::string data = notify.SerializeAsString();

    std::lock_guard<std::mutex> lock(ws_mutex_);
    for (const auto& uid : response.target_ids()) {
        auto it = uid_conns_.find(uid);
        if (it != uid_conns_.end()) {
            it->second->send(data, websocketpp::frame::opcode::binary);
        }
    }
}

} // namespace im

/**
 * @brief 程序入口。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    im::init_logger(FLAGS_run_mode, FLAGS_log_dir, FLAGS_log_level);

    auto redis = im::RedisClientFactory::create(FLAGS_redis_host, FLAGS_redis_port,
                                                FLAGS_redis_db, true);
    auto channels = std::make_shared<im::ServiceManager>(FLAGS_service_base);
    channels->follow(FLAGS_user_service);
    channels->follow(FLAGS_message_service);
    channels->follow(FLAGS_friend_service);
    channels->follow(FLAGS_transmit_service);
    channels->follow(FLAGS_file_service);
    auto discovery = std::make_shared<im::Discovery>(
        FLAGS_etcd_host, FLAGS_service_base,
        [channels](const std::string& key, const std::string& host) {
            channels->onServiceOnline(key, host);
        },
        [channels](const std::string& key, const std::string& host) {
            channels->onServiceOffline(key, host);
        });

    im::GatewayServer gateway(FLAGS_http_port, FLAGS_ws_port, redis, channels,
                              FLAGS_user_service, FLAGS_friend_service,
                              FLAGS_transmit_service, FLAGS_message_service,
                              FLAGS_file_service);
    gateway.start();
    return 0;
}
