/**
 * @file message_server.hpp
 * @brief 消息存储服务声明（实现见 services/message/src/message_server.cpp）。
 */

#pragma once

#include <memory>
#include <string>

#include <odb/database.hxx>

#include "db/message_dao.hpp"
#include "es.hpp"
#include "mq.hpp"

namespace im {

/**
 * @brief 消息消费者：消费 MQ 中的 MessageInfo，落 MySQL 与 ES。
 */
class MessageConsumer {
public:
    MessageConsumer(std::shared_ptr<odb::core::database> db,
                    std::shared_ptr<ESClient> es,
                    MQClient::Ptr mq,
                    std::string queue);

    /// 开始消费
    void start();

private:
    /// 处理一条 MQ 消息
    void onMessage(const char* body, size_t size);

    std::shared_ptr<odb::core::database> db_;
    std::shared_ptr<MessageDao> messages_;
    std::shared_ptr<ESClient> es_;
    MQClient::Ptr mq_;
    std::string queue_;
};

} // namespace im
