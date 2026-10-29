/**
 * @file transmit_server.hpp
 * @brief 消息转发服务实现类声明（实现见 services/transmit/src/transmit_server.cpp）。
 */

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>

#include <odb/database.hxx>

#include "channel.hpp"
#include "db/chat_session_member_dao.hpp"
#include "mq.hpp"
#include "transmit.pb.h"

namespace im {

/**
 * @brief 消息转发服务：组装消息、校验成员、投递 MQ、返回推送目标。
 */
class TransmitServiceImpl final : public TransmitService {
public:
    TransmitServiceImpl(std::shared_ptr<odb::core::database> db,
                        ServiceManager::Ptr channels,
                        std::vector<MQClient::Ptr> mqs,
                        std::string user_service,
                        std::string exchange,
                        std::string routing_key);

    void SendMessage(google::protobuf::RpcController* controller,
                     const SendMessageReq* request,
                     SendMessageRsp* response,
                     google::protobuf::Closure* done) override;

private:
    std::shared_ptr<odb::core::database> db_;
    std::shared_ptr<ChatSessionMemberDao> members_;
    ServiceManager::Ptr channels_;
    /// 会话成员缓存（30 秒），避免每条消息查 MySQL
    struct MemberCache {
        std::vector<std::string> ids;
        std::chrono::steady_clock::time_point expire;
    };
    /// 发送者信息缓存（60 秒），避免每条消息调 user 服务
    struct SenderCache {
        UserInfo info;
        std::chrono::steady_clock::time_point expire;
    };

    std::vector<std::string> cachedMembers(const std::string& session_id);
    bool cachedSender(const std::string& request_id, const std::string& user_id,
                      UserInfo& info);

    std::mutex cache_mutex_;
    std::unordered_map<std::string, MemberCache> member_cache_;
    std::unordered_map<std::string, SenderCache> sender_cache_;

    std::vector<MQClient::Ptr> mqs_;
    std::atomic<size_t> mq_index_{0};
    std::string user_service_;
    std::string exchange_;
    std::string routing_key_;
};

} // namespace im
