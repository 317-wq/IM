/**
 * @file mq.cpp
 * @brief RabbitMQ 客户端封装实现。
 */

#include "mq.hpp"

#include <cstdlib>

#include "logger.hpp"

namespace im {

/**
 * @brief 实现构造：建立连接与信道，启动 libev 事件循环线程。
 */
MQClient::MQClient(const std::string& user, const std::string& password,
                   const std::string& host) {
    loop_ = EV_DEFAULT;
    handler_ = std::make_unique<AMQP::LibEvHandler>(loop_);

    const std::string url = "amqp://" + user + ":" + password + "@" + host + "/";
    AMQP::Address address(url);
    connection_ = std::make_unique<AMQP::TcpConnection>(handler_.get(), address);
    channel_ = std::make_unique<AMQP::TcpChannel>(connection_.get());

    ev_async_init(&async_, stopCallback);
    ev_async_start(loop_, &async_);

    loop_thread_ = std::thread([this]() {
        running_ = true;
        ev_run(loop_, 0);
        running_ = false;
    });
}

/**
 * @brief 实现析构：通知事件循环退出并等待线程结束。
 */
MQClient::~MQClient() {
    if (loop_ != nullptr) {
        ev_async_send(loop_, &async_);
    }
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}

/**
 * @brief 实现交换机/队列声明与绑定。
 */
void MQClient::declareComponents(const std::string& exchange,
                                 const std::string& queue,
                                 const std::string& routing_key) {
    channel_->declareExchange(exchange, AMQP::ExchangeType::direct)
        .onError([exchange](const char* message) {
            IM_ERROR("declare exchange {} failed: {}", exchange, message);
        })
        .onSuccess([exchange]() { IM_INFO("exchange {} ready", exchange); });

    channel_->declareQueue(queue, AMQP::durable)
        .onError([queue](const char* message) {
            IM_ERROR("declare queue {} failed: {}", queue, message);
        })
        .onSuccess([queue]() { IM_INFO("queue {} ready", queue); });

    channel_->bindQueue(exchange, queue, routing_key)
        .onError([exchange, queue](const char* message) {
            IM_ERROR("bind {} -> {} failed: {}", exchange, queue, message);
        })
        .onSuccess([exchange, queue]() {
            IM_INFO("bind {} -> {} ready", exchange, queue);
        });
}

/**
 * @brief 实现投递。
 */
bool MQClient::publish(const std::string& exchange,
                       const std::string& message,
                       const std::string& routing_key) {
    // AMQP-CPP 的 Channel 不是线程安全的：RPC 线程并发 publish 会偶发失败/重复。
    std::lock_guard<std::mutex> lock(publish_mutex_);
    if (!channel_->publish(exchange, routing_key, message)) {
        IM_ERROR("publish to {} failed", exchange);
        return false;
    }
    return true;
}

/**
 * @brief 实现订阅：收到消息后回调并自动 ack。
 */
void MQClient::consume(const std::string& queue, const MessageCallback& callback) {
    channel_->consume(queue, "im-consumer")
        .onReceived([this, callback](const AMQP::Message& message, uint64_t tag,
                                     bool /*redelivered*/) {
            callback(message.body(), static_cast<size_t>(message.bodySize()));
            channel_->ack(tag);
        })
        .onError([queue](const char* message) {
            IM_ERROR("consume {} failed: {}", queue, message);
        });
}

/**
 * @brief 实现退出回调：收到 async 信号后 break 事件循环。
 */
void MQClient::stopCallback(struct ev_loop* loop, ev_async* /*watcher*/, int /*revents*/) {
    ev_break(loop, EVBREAK_ALL);
}

} // namespace im
