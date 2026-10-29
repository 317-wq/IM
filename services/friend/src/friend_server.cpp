/**
 * @file friend_server.cpp
 * @brief 好友与会话服务实现与程序入口。
 */

#include "friend_server.hpp"

#include <algorithm>
#include <cstdlib>

#include <brpc/server.h>
#include <gflags/gflags.h>

#include "etcd.hpp"
#include "logger.hpp"
#include "mysql.hpp"
#include "user.pb.h"
#include "utils.hpp"

DEFINE_bool(run_mode, false, "运行模式：false-调试（控制台），true-发布（文件）");
DEFINE_string(log_dir, "logs/friend", "发布模式下的日志目录");
DEFINE_int32(log_level, 0, "日志等级");
DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd 地址");
DEFINE_string(service_base, "/im/service", "服务注册根目录");
DEFINE_string(service_name, "friend", "本服务名称");
DEFINE_string(instance_id, "", "实例标识，空则使用 advertise_host");
DEFINE_string(advertise_host, "127.0.0.1:10003", "对外访问地址 host:port");
DEFINE_int32(listen_port, 10003, "RPC 监听端口");
DEFINE_int32(rpc_threads, 4, "RPC IO 线程数");
DEFINE_string(user_service, "/im/service/user", "用户服务名");
DEFINE_string(mysql_host, "127.0.0.1", "MySQL 地址");
DEFINE_int32(mysql_port, 3306, "MySQL 端口");
DEFINE_string(mysql_user, "root", "MySQL 用户名");
DEFINE_string(mysql_password, "", "MySQL 密码；为空时读 IM_MYSQL_PASSWORD");
DEFINE_string(mysql_db, "im", "MySQL 数据库名");
DEFINE_string(mysql_charset, "utf8mb4", "MySQL 字符集");
DEFINE_int32(mysql_pool, 4, "连接池大小");
DEFINE_string(es_host, "http://127.0.0.1:9200", "Elasticsearch 地址");
DEFINE_string(es_index, "im_user", "用户搜索索引名");

namespace im {

namespace {

/**
 * @brief 从环境变量读取 MySQL 密码。
 */
std::string mysqlPassword() {
    if (!FLAGS_mysql_password.empty()) {
        return FLAGS_mysql_password;
    }
    const char* env = std::getenv("IM_MYSQL_PASSWORD");
    return env == nullptr ? std::string() : std::string(env);
}

} // namespace

/**
 * @brief 实现构造。
 */
FriendServiceImpl::FriendServiceImpl(std::shared_ptr<odb::core::database> db,
                                     ServiceManager::Ptr channels,
                                     std::shared_ptr<ESClient> es,
                                     std::string user_service)
    : db_(std::move(db)),
      relations_(std::make_shared<RelationDao>(db_)),
      applies_(std::make_shared<FriendApplyDao>(db_)),
      sessions_(std::make_shared<ChatSessionDao>(db_)),
      members_(std::make_shared<ChatSessionMemberDao>(db_)),
      repository_(std::make_shared<FriendRepository>(db_)),
      channels_(std::move(channels)),
      es_(std::move(es)),
      user_service_(std::move(user_service)) {}

/**
 * @brief 实现批量拉取用户信息（每 100 个一组）。
 */
bool FriendServiceImpl::fetchUsers(const std::string& request_id,
                                   const std::vector<std::string>& user_ids,
                                   std::vector<UserInfo>& users) {
    users.clear();
    constexpr size_t kBatch = 100;

    for (size_t begin = 0; begin < user_ids.size(); begin += kBatch) {
        const size_t end = std::min(begin + kBatch, user_ids.size());
        auto channel = channels_->choose(user_service_);
        if (!channel) {
            IM_ERROR("{} no user service node", request_id);
            return false;
        }

        GetMultiUserInfoReq request;
        request.set_request_id(request_id);
        for (size_t i = begin; i < end; ++i) {
            request.add_user_ids(user_ids[i]);
        }

        GetMultiUserInfoRsp response;
        UserService_Stub stub(channel.get());
        brpc::Controller cntl;
        stub.GetMultiUserInfo(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            IM_ERROR("{} batch get user info failed", request_id);
            return false;
        }

        for (size_t i = begin; i < end; ++i) {
            auto it = response.users().find(user_ids[i]);
            if (it != response.users().end()) {
                users.push_back(it->second);
            }
        }
    }
    return true;
}

/**
 * @brief 实现好友列表。
 */
void FriendServiceImpl::GetFriendList(google::protobuf::RpcController*,
                                      const GetFriendListReq* request,
                                      GetFriendListRsp* response,
                                      google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());
    if (request->user_id().empty()) {
        response->set_success(false);
        response->set_errmsg("未登录或会话已过期");
        return;
    }

    auto ids = relations_->friends(request->user_id());
    std::vector<UserInfo> users;
    if (!fetchUsers(request->request_id(), ids, users)) {
        response->set_success(false);
        response->set_errmsg("获取用户信息失败");
        return;
    }
    for (auto& user : users) {
        response->add_friends()->Swap(&user);
    }
    response->set_success(true);
}

/**
 * @brief 实现发送好友申请。
 */
void FriendServiceImpl::FriendAdd(google::protobuf::RpcController*,
                                  const FriendAddReq* request,
                                  FriendAddRsp* response,
                                  google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    auto fail = [response](const std::string& msg) {
        response->set_success(false);
        response->set_errmsg(msg);
    };

    const std::string& uid = request->user_id();
    const std::string& peer = request->respondent_id();
    if (uid.empty()) {
        return fail("未登录或会话已过期");
    }
    if (peer.empty() || peer == uid) {
        return fail("被申请人无效");
    }
    if (relations_->exists(uid, peer)) {
        return fail("已经是好友");
    }
    if (applies_->exists(uid, peer)) {
        return fail("已经申请过");
    }

    std::vector<std::string> ids{peer};
    std::vector<UserInfo> users;
    if (!fetchUsers(request->request_id(), ids, users) || users.empty()) {
        return fail("被申请人不存在");
    }

    FriendApply apply(uuid(), uid, peer);
    if (!applies_->insert(apply)) {
        return fail("发送申请失败");
    }
    response->set_success(true);
    response->set_notify_event_id(apply.event_id());
    IM_INFO("friend apply {} -> {}", uid, peer);
}

/**
 * @brief 实现处理申请：同意走跨表事务，拒绝只删申请。
 */
void FriendServiceImpl::FriendAddProcess(google::protobuf::RpcController*,
                                         const FriendAddProcessReq* request,
                                         FriendAddProcessRsp* response,
                                         google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    auto fail = [response](const std::string& msg) {
        response->set_success(false);
        response->set_errmsg(msg);
    };

    const std::string& uid = request->user_id();
    const std::string& applicant = request->apply_user_id();
    if (uid.empty()) {
        return fail("未登录或会话已过期");
    }
    if (!applies_->exists(applicant, uid)) {
        return fail("申请不存在或已处理");
    }

    if (!request->agree()) {
        if (!applies_->remove(applicant, uid)) {
            return fail("处理申请失败");
        }
        response->set_success(true);
        return;
    }

    const std::string session_id = uuid();
    if (!repository_->acceptFriend(applicant, uid, session_id)) {
        return fail("同意好友失败");
    }
    response->set_success(true);
    response->set_new_session_id(session_id);
    IM_INFO("friend accept {} <-> {}, session={}", applicant, uid, session_id);
}

/**
 * @brief 实现待处理申请列表。
 */
void FriendServiceImpl::GetPendingFriendEventList(
    google::protobuf::RpcController*,
    const GetPendingFriendEventListReq* request,
    GetPendingFriendEventListRsp* response,
    google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());
    if (request->user_id().empty()) {
        response->set_success(false);
        response->set_errmsg("未登录或会话已过期");
        return;
    }

    auto applies = applies_->listByRespondent(request->user_id());
    std::vector<std::string> ids;
    for (const auto& apply : applies) {
        ids.push_back(apply.from_user());
    }
    std::vector<UserInfo> users;
    if (!fetchUsers(request->request_id(), ids, users)) {
        response->set_success(false);
        response->set_errmsg("获取申请人信息失败");
        return;
    }

    for (size_t i = 0; i < applies.size() && i < users.size(); ++i) {
        auto event = response->add_events();
        event->set_event_id(applies[i].event_id());
        event->mutable_sender()->CopyFrom(users[i]);
    }
    response->set_success(true);
}

/**
 * @brief 实现删除好友。
 */
void FriendServiceImpl::FriendRemove(google::protobuf::RpcController*,
                                     const FriendRemoveReq* request,
                                     FriendRemoveRsp* response,
                                     google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());
    if (request->user_id().empty()) {
        response->set_success(false);
        response->set_errmsg("未登录或会话已过期");
        return;
    }
    if (!relations_->removePair(request->user_id(), request->peer_id())) {
        response->set_success(false);
        response->set_errmsg("删除好友失败");
        return;
    }
    response->set_success(true);
}

/**
 * @brief 实现用户搜索：ES 检索后到 user 服务补全。
 */
void FriendServiceImpl::FriendSearch(google::protobuf::RpcController*,
                                     const FriendSearchReq* request,
                                     FriendSearchRsp* response,
                                     google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());
    if (request->user_id().empty()) {
        response->set_success(false);
        response->set_errmsg("未登录或会话已过期");
        return;
    }

    auto docs = es_->searchUsers(request->search_key(), request->user_id(), 20);
    std::vector<std::string> ids;
    for (const auto& doc : docs) {
        ids.push_back(doc.user_id);
    }

    std::vector<UserInfo> users;
    if (!fetchUsers(request->request_id(), ids, users)) {
        response->set_success(false);
        response->set_errmsg("获取搜索结果失败");
        return;
    }
    for (auto& user : users) {
        response->add_users()->Swap(&user);
    }
    response->set_success(true);
}

/**
 * @brief 实现会话列表：单聊补对方信息，群聊返回群名。
 */
void FriendServiceImpl::GetChatSessionList(google::protobuf::RpcController*,
                                           const GetChatSessionListReq* request,
                                           GetChatSessionListRsp* response,
                                           google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());
    if (request->user_id().empty()) {
        response->set_success(false);
        response->set_errmsg("未登录或会话已过期");
        return;
    }

    const std::string& uid = request->user_id();
    for (const auto& session_id : members_->sessionIds(uid)) {
        auto session = sessions_->findById(session_id);
        if (session == nullptr) {
            continue;
        }

        ChatSessionInfo* info = response->add_sessions();
        info->set_session_id(session->session_id());
        info->set_name(session->name());
        info->set_type(static_cast<int32_t>(session->type()));

        if (session->type() != ChatSessionType::SINGLE) {
            continue;
        }

        std::string peer_id;
        for (const auto& member_id : members_->memberIds(session_id)) {
            if (member_id != uid) {
                peer_id = member_id;
                break;
            }
        }
        if (peer_id.empty()) {
            continue;
        }
        info->set_single_chat_friend_id(peer_id);

        std::vector<UserInfo> users;
        if (fetchUsers(request->request_id(), {peer_id}, users) && !users.empty()) {
            info->mutable_single_chat_friend()->CopyFrom(users[0]);
            info->set_name(users[0].nickname());
        }
    }
    response->set_success(true);
}

/**
 * @brief 实现创建群聊：成员列表必须包含创建者。
 */
void FriendServiceImpl::ChatSessionCreate(google::protobuf::RpcController*,
                                          const ChatSessionCreateReq* request,
                                          ChatSessionCreateRsp* response,
                                          google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());
    const std::string& uid = request->user_id();
    if (uid.empty()) {
        response->set_success(false);
        response->set_errmsg("未登录或会话已过期");
        return;
    }

    std::vector<std::string> member_ids;
    bool contains_creator = false;
    for (const auto& member_id : request->member_ids()) {
        member_ids.push_back(member_id);
        if (member_id == uid) {
            contains_creator = true;
        }
    }
    if (!contains_creator) {
        response->set_success(false);
        response->set_errmsg("成员列表必须包含创建者");
        return;
    }

    const std::string session_id = uuid();
    if (!repository_->createGroup(session_id, request->chat_session_name(), member_ids)) {
        response->set_success(false);
        response->set_errmsg("创建群聊失败");
        return;
    }

    response->set_success(true);
    response->set_chat_session_id(session_id);
}

/**
 * @brief 实现会话成员列表。
 */
void FriendServiceImpl::GetChatSessionMember(
    google::protobuf::RpcController*,
    const GetChatSessionMemberReq* request,
    GetChatSessionMemberRsp* response,
    google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());
    if (request->user_id().empty()) {
        response->set_success(false);
        response->set_errmsg("未登录或会话已过期");
        return;
    }

    auto ids = members_->memberIds(request->chat_session_id());
    std::vector<UserInfo> users;
    if (!fetchUsers(request->request_id(), ids, users)) {
        response->set_success(false);
        response->set_errmsg("获取成员信息失败");
        return;
    }
    for (auto& user : users) {
        response->add_members()->Swap(&user);
    }
    response->set_success(true);
}

} // namespace im

/**
 * @brief 程序入口。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    im::init_logger(FLAGS_run_mode, FLAGS_log_dir, FLAGS_log_level);

    auto db = im::ODBFactory::create(FLAGS_mysql_user, im::mysqlPassword(),
                                     FLAGS_mysql_host, FLAGS_mysql_db,
                                     FLAGS_mysql_charset,
                                     static_cast<unsigned int>(FLAGS_mysql_port),
                                     FLAGS_mysql_pool);
    auto es = std::make_shared<im::ESClient>(FLAGS_es_host, FLAGS_es_index);

    auto channels = std::make_shared<im::ServiceManager>(FLAGS_service_base);
    channels->follow(FLAGS_user_service);
    auto discovery = std::make_shared<im::Discovery>(
        FLAGS_etcd_host, FLAGS_service_base,
        [channels](const std::string& key, const std::string& host) {
            channels->onServiceOnline(key, host);
        },
        [channels](const std::string& key, const std::string& host) {
            channels->onServiceOffline(key, host);
        });

    brpc::Server server;
    im::FriendServiceImpl service(db, channels, es, FLAGS_user_service);
    if (server.AddService(&service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        IM_FATAL("add friend service failed");
        return -1;
    }

    brpc::ServerOptions options;
    options.num_threads = FLAGS_rpc_threads;
    if (server.Start(FLAGS_listen_port, &options) != 0) {
        IM_FATAL("start friend server on port {} failed", FLAGS_listen_port);
        return -1;
    }

    const std::string instance =
        FLAGS_instance_id.empty() ? FLAGS_advertise_host : FLAGS_instance_id;
    im::Registry registry(FLAGS_etcd_host, FLAGS_service_base);
    if (!registry.registerService(FLAGS_service_name, instance, FLAGS_advertise_host)) {
        IM_FATAL("register service {} failed", FLAGS_service_name);
        return -1;
    }

    IM_INFO("friend_server listening on 0.0.0.0:{}", FLAGS_listen_port);
    server.RunUntilAskedToQuit();
    return 0;
}
