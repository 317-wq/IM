/**
 * @file mq.hpp
 * @brief RabbitMQ 客户端封装声明（实现见 common/src/mq.cpp）。
 *
 * 基于 AMQP-CPP + libev：事件循环跑在独立线程，业务线程调用 publish/consume。
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <thread>

#include <amqpcpp.h>
#include <amqpcpp/libev.h>
#include <ev.h>

namespace im {

/**
 * @brief 消息队列客户端。
 */
class MQClient {
public:
    using Ptr = std::shared_ptr<MQClient>;
    using MessageCallback = std::function<void(const char*, size_t)>;

    /**
     * @param user     用户名
     * @param password 密码
     * @param host     地址，如 127.0.0.1:5672
     */
    MQClient(const std::string& user, const std::string& password, const std::string& host);

    ~MQClient();

    /**
     * @brief 声明交换机、队列并绑定。
     */
    void declareComponents(const std::string& exchange,
                           const std::string& queue,
                           const std::string& routing_key);

    /**
     * @brief 投递消息。
     */
    bool publish(const std::string& exchange,
                 const std::string& message,
                 const std::string& routing_key);

    /**
     * @brief 订阅队列（M6 消息存储使用）。
     */
    void consume(const std::string& queue, const MessageCallback& callback);

private:
    static void stopCallback(struct ev_loop* loop, ev_async* watcher, int revents);

    struct ev_loop* loop_;
    ev_async async_;
    std::unique_ptr<AMQP::LibEvHandler> handler_;
    std::unique_ptr<AMQP::TcpConnection> connection_;
    std::unique_ptr<AMQP::TcpChannel> channel_;
    std::thread loop_thread_;
    std::mutex publish_mutex_;
    bool running_ = false;
};

} // namespace im
