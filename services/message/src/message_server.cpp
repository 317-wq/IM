/**
 * @file message_server.cpp
 * @brief 消息存储服务：消费 RabbitMQ，落 MySQL 并写 ES 索引。
 */

#include "message_server.hpp"

#include <cstdlib>
#include <unistd.h>

#include <brpc/server.h>
#include <gflags/gflags.h>

#include "base.pb.h"
#include "channel.hpp"
#include "etcd.hpp"
#include "logger.hpp"
#include "message_service.hpp"
#include "mysql.hpp"

DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/message", "发布模式日志目录");
DEFINE_int32(log_level, 0, "日志等级");
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
DEFINE_string(mq_queue, "im.msg.persist", "持久化消息队列");
DEFINE_string(mq_routing_key, "im.msg.persist", "路由键");
DEFINE_string(es_host, "http://127.0.0.1:9200", "Elasticsearch 地址");
DEFINE_string(es_index, "im_message", "消息索引名");

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd 地址");
DEFINE_string(service_base, "/im/service", "服务注册根目录");
DEFINE_string(service_name, "message", "本服务名称");
DEFINE_string(instance_id, "", "实例标识");
DEFINE_string(advertise_host, "127.0.0.1:10005", "对外访问地址");
DEFINE_int32(listen_port, 10005, "RPC 监听端口");
DEFINE_int32(rpc_threads, 4, "RPC IO 线程数");
DEFINE_string(user_service, "/im/service/user", "用户服务名");

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
MessageConsumer::MessageConsumer(std::shared_ptr<odb::core::database> db,
                                 std::shared_ptr<ESClient> es,
                                 MQClient::Ptr mq,
                                 std::string queue)
    : db_(std::move(db)),
      messages_(std::make_shared<MessageDao>(db_)),
      es_(std::move(es)),
      mq_(std::move(mq)),
      queue_(std::move(queue)) {}

/**
 * @brief 实现启动：注册消费回调。
 */
void MessageConsumer::start() {
    mq_->consume(queue_, [this](const char* body, size_t size) {
        onMessage(body, size);
    });
    IM_INFO("message consumer started on {}", queue_);
}

/**
 * @brief 实现消息处理：文本取 content，其他类型取 file_id 元信息。
 */
void MessageConsumer::onMessage(const char* body, size_t size) {
    MessageInfo info;
    if (!info.ParseFromArray(body, static_cast<int>(size))) {
        IM_ERROR("parse message failed");
        return;
    }

    Message message(info.message_id(), info.chat_session_id(), info.sender().user_id(),
                    static_cast<unsigned char>(info.message().message_type()),
                    boost::posix_time::from_time_t(info.timestamp()));

    std::string content;
    switch (info.message().message_type()) {
        case MessageType::STRING:
            content = info.message().string_message().content();
            message.content(content);
            break;
        case MessageType::IMAGE:
            message.file_id(info.message().image_message().file_id());
            break;
        case MessageType::FILE:
            message.file_id(info.message().file_message().file_id());
            message.file_name(info.message().file_message().file_name());
            message.file_size(static_cast<unsigned int>(
                info.message().file_message().file_size()));
            break;
        case MessageType::SPEECH:
            message.file_id(info.message().speech_message().file_id());
            break;
        default:
            IM_ERROR("unknown message type {}", info.message().message_type());
            return;
    }

    if (!messages_->insert(message)) {
        return;
    }
    es_->indexMessage(message.message_id(), message.session_id(), message.sender_id(),
                      content, info.timestamp());
    IM_INFO("stored message {} session={}", message.message_id(), message.session_id());
}

} // namespace im

/**
 * @brief 程序入口：初始化依赖 -> 开始消费 -> 阻塞。
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
    es->ensureMessageIndex();

    auto mq = std::make_shared<im::MQClient>(FLAGS_mq_user, FLAGS_mq_password,
                                             FLAGS_mq_host);
    mq->declareComponents(FLAGS_mq_exchange, FLAGS_mq_queue, FLAGS_mq_routing_key);

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

    auto consumer = std::make_shared<im::MessageConsumer>(db, es, mq, FLAGS_mq_queue);
    consumer->start();

    brpc::Server server;
    im::MessageServiceImpl service(db, es, channels, FLAGS_user_service);
    if (server.AddService(&service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        IM_FATAL("add message service failed");
        return -1;
    }
    brpc::ServerOptions options;
    options.num_threads = FLAGS_rpc_threads;
    if (server.Start(FLAGS_listen_port, &options) != 0) {
        IM_FATAL("start message server failed");
        return -1;
    }

    const std::string instance =
        FLAGS_instance_id.empty() ? FLAGS_advertise_host : FLAGS_instance_id;
    im::Registry registry(FLAGS_etcd_host, FLAGS_service_base);
    if (!registry.registerService(FLAGS_service_name, instance, FLAGS_advertise_host)) {
        IM_FATAL("register message service failed");
        return -1;
    }

    IM_INFO("message_server listening on 0.0.0.0:{}", FLAGS_listen_port);
    server.RunUntilAskedToQuit();
    return 0;
}
