/**
 * @file gateway_server.cpp
 * @brief HTTP 网关实现（M7 第一步：注册/登录/用户信息/历史消息）。
 */

#include "gateway_server.hpp"

#include <chrono>

#include <gflags/gflags.h>

#include "etcd.hpp"
#include "logger.hpp"
#include "message.pb.h"
#include "user.pb.h"

DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/gateway", "发布模式日志目录");
DEFINE_int32(log_level, 0, "日志等级");
DEFINE_int32(http_port, 9000, "HTTP 监听端口");
DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd 地址");
DEFINE_string(service_base, "/im/service", "服务注册根目录");
DEFINE_string(user_service, "/im/service/user", "用户服务名");
DEFINE_string(message_service, "/im/service/message", "消息服务名");
DEFINE_string(redis_host, "127.0.0.1", "Redis 地址");
DEFINE_int32(redis_port, 6379, "Redis 端口");
DEFINE_int32(redis_db, 0, "Redis 库号");

namespace im {

/**
 * @brief 实现构造并注册路由。
 */
GatewayServer::GatewayServer(int port,
                             std::shared_ptr<sw::redis::Redis> redis,
                             ServiceManager::Ptr channels,
                             std::string user_service,
                             std::string message_service)
    : sessions_(std::make_shared<SessionStore>(std::move(redis),
                                               std::chrono::seconds(7 * 24 * 3600))),
      channels_(std::move(channels)),
      user_service_(std::move(user_service)),
      message_service_(std::move(message_service)) {
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
    IM_INFO("gateway listening on 0.0.0.0:{}", FLAGS_http_port);
    http_.listen("0.0.0.0", FLAGS_http_port);
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
    auto discovery = std::make_shared<im::Discovery>(
        FLAGS_etcd_host, FLAGS_service_base,
        [channels](const std::string& key, const std::string& host) {
            channels->onServiceOnline(key, host);
        },
        [channels](const std::string& key, const std::string& host) {
            channels->onServiceOffline(key, host);
        });

    im::GatewayServer gateway(FLAGS_http_port, redis, channels, FLAGS_user_service,
                              FLAGS_message_service);
    gateway.start();
    return 0;
}
