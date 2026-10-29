/**
 * @file message_server.hpp
 * @brief 消息存储服务声明（实现见 services/message/src/message_server.cpp）。
 */

#pragma once

#include <memory>
#include <string>

#include <odb/database.hxx>

#include "channel.hpp"
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
                    std::string queue,
                    ServiceManager::Ptr channels,
                    std::string file_service);

    /// 开始消费
    void start();

private:
    /// 处理一条 MQ 消息
    void onMessage(const char* body, size_t size);

    /// 把二进制内容上传到 file 服务，返回 file_id
    bool uploadFile(const std::string& request_id,
                    const std::string& content,
                    const std::string& file_name,
                    std::string& file_id);

    std::shared_ptr<odb::core::database> db_;
    std::shared_ptr<MessageDao> messages_;
    std::shared_ptr<ESClient> es_;
    MQClient::Ptr mq_;
    std::string queue_;
    ServiceManager::Ptr channels_;
    std::string file_service_;
};

} // namespace im
