/**
 * @file transmit_server.cpp
 * @brief 消息转发服务实现与程序入口。
 */

#include "transmit_server.hpp"

#include <algorithm>
#include <cstdlib>

#include <brpc/server.h>
#include <gflags/gflags.h>

#include "etcd.hpp"
#include "logger.hpp"
#include "mysql.hpp"
#include "user.pb.h"
#include "utils.hpp"

DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/transmit", "发布模式日志目录");
DEFINE_int32(log_level, 0, "日志等级");
DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd 地址");
DEFINE_string(service_base, "/im/service", "服务注册根目录");
DEFINE_string(service_name, "transmit", "本服务名称");
DEFINE_string(instance_id, "", "实例标识");
DEFINE_string(advertise_host, "127.0.0.1:10004", "对外访问地址");
DEFINE_int32(listen_port, 10004, "RPC 监听端口");
DEFINE_int32(rpc_threads, 4, "RPC IO 线程数");
DEFINE_string(user_service, "/im/service/user", "用户服务名");
DEFINE_string(mysql_host, "127.0.0.1", "MySQL 地址");
DEFINE_int32(mysql_port, 3306, "MySQL 端口");
DEFINE_string(mysql_user, "root", "MySQL 用户名");
DEFINE_string(mysql_password, "", "MySQL 密码；为空读 IM_MYSQL_PASSWORD");
DEFINE_string(mysql_db, "im", "MySQL 数据库名");
DEFINE_string(mysql_charset, "utf8mb4", "MySQL 字符集");
DEFINE_int32(mysql_pool, 4, "连接池大小");
DEFINE_string(mq_user, "guest", "RabbitMQ 用户名");
DEFINE_string(mq_password, "guest", "RabbitMQ 密码");
DEFINE_string(mq_host, "127.0.0.1:5672", "RabbitMQ 地址");
DEFINE_string(mq_exchange, "im.msg", "消息交换机");
DEFINE_string(mq_queue, "im.msg.persist", "持久化队列");
DEFINE_string(mq_routing_key, "im.msg.persist", "路由键");
DEFINE_int32(mq_publishers, 4, "发布用 MQ 连接/信道数量");

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
TransmitServiceImpl::TransmitServiceImpl(std::shared_ptr<odb::core::database> db,
                                         ServiceManager::Ptr channels,
                                         std::vector<MQClient::Ptr> mqs,
                                         std::string user_service,
                                         std::string exchange,
                                         std::string routing_key)
    : db_(std::move(db)),
      members_(std::make_shared<ChatSessionMemberDao>(db_)),
      channels_(std::move(channels)),
      mqs_(std::move(mqs)),
      user_service_(std::move(user_service)),
      exchange_(std::move(exchange)),
      routing_key_(std::move(routing_key)) {}

/**
 * @brief 实现发送：成员校验 -> 取发送者 -> 组装 -> 投递 MQ。
 */
void TransmitServiceImpl::SendMessage(google::protobuf::RpcController*,
                                      const SendMessageReq* request,
                                      SendMessageRsp* response,
                                      google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    auto fail = [response](const std::string& msg) {
        response->set_success(false);
        response->set_errmsg(msg);
    };

    const std::string& uid = request->user_id();
    const std::string& session_id = request->chat_session_id();
    if (uid.empty()) {
        return fail("未登录或会话已过期");
    }
    if (session_id.empty()) {
        return fail("会话 ID 不能为空");
    }

    auto member_ids = cachedMembers(session_id);
    if (std::find(member_ids.begin(), member_ids.end(), uid) == member_ids.end()) {
        return fail("无权在该会话发送消息");
    }

    UserInfo sender;
    if (!cachedSender(request->request_id(), uid, sender)) {
        return fail("获取发送者信息失败");
    }

    MessageInfo message;
    message.set_message_id(uuid());
    message.set_chat_session_id(session_id);
    message.set_timestamp(now_s());
    message.mutable_sender()->CopyFrom(sender);
    message.mutable_message()->CopyFrom(request->message());

    auto mq = mqs_[mq_index_.fetch_add(1) % mqs_.size()];
    if (!mq->publish(exchange_, message.SerializeAsString(), routing_key_)) {
        return fail("消息投递失败");
    }

    response->set_success(true);
    response->mutable_message()->CopyFrom(message);
    for (const auto& member_id : member_ids) {
        response->add_target_ids(member_id);
    }
    IM_INFO("transmit session={} message={} targets={}", session_id, message.message_id(),
            member_ids.size());
}

std::vector<std::string> TransmitServiceImpl::cachedMembers(
    const std::string& session_id) {
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = member_cache_.find(session_id);
        if (it != member_cache_.end() && it->second.expire > now) {
            return it->second.ids;
        }
    }
    auto ids = members_->memberIds(session_id);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        member_cache_[session_id] = MemberCache{ids, now + std::chrono::seconds(30)};
    }
    return ids;
}

bool TransmitServiceImpl::cachedSender(const std::string& request_id,
                                       const std::string& user_id,
                                       UserInfo& info) {
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = sender_cache_.find(user_id);
        if (it != sender_cache_.end() && it->second.expire > now) {
            info = it->second.info;
            return true;
        }
    }

    auto channel = channels_->choose(user_service_);
    if (!channel) {
        return false;
    }
    GetUserInfoReq request;
    request.set_request_id(request_id);
    request.set_user_id(user_id);
    GetUserInfoRsp response;
    UserService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetUserInfo(&cntl, &request, &response, nullptr);
    if (cntl.Failed() || !response.success()) {
        return false;
    }
    info = response.user_info();
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        sender_cache_[user_id] = SenderCache{info, now + std::chrono::seconds(60)};
    }
    return true;
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
    const int publisher_count = FLAGS_mq_publishers > 0 ? FLAGS_mq_publishers : 1;
    std::vector<im::MQClient::Ptr> mqs;
    for (int i = 0; i < publisher_count; ++i) {
        auto mq = std::make_shared<im::MQClient>(FLAGS_mq_user, FLAGS_mq_password,
                                                 FLAGS_mq_host);
        mq->declareComponents(FLAGS_mq_exchange, FLAGS_mq_queue, FLAGS_mq_routing_key);
        mqs.push_back(mq);
    }

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
    im::TransmitServiceImpl service(db, channels, std::move(mqs), FLAGS_user_service,
                                    FLAGS_mq_exchange, FLAGS_mq_routing_key);
    if (server.AddService(&service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        IM_FATAL("add transmit service failed");
        return -1;
    }

    brpc::ServerOptions options;
    options.num_threads = FLAGS_rpc_threads;
    if (server.Start(FLAGS_listen_port, &options) != 0) {
        IM_FATAL("start transmit server failed");
        return -1;
    }

    const std::string instance =
        FLAGS_instance_id.empty() ? FLAGS_advertise_host : FLAGS_instance_id;
    im::Registry registry(FLAGS_etcd_host, FLAGS_service_base);
    if (!registry.registerService(FLAGS_service_name, instance, FLAGS_advertise_host)) {
        IM_FATAL("register service {} failed", FLAGS_service_name);
        return -1;
    }

    IM_INFO("transmit_server listening on 0.0.0.0:{}", FLAGS_listen_port);
    server.RunUntilAskedToQuit();
    return 0;
}
