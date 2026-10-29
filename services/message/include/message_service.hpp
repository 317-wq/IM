/**
 * @file message_service.hpp
 * @brief 消息查询 RPC 服务声明（实现见 services/message/src/message_service.cpp）。
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <odb/database.hxx>

#include "channel.hpp"
#include "db/message_dao.hpp"
#include "es.hpp"
#include "message.pb.h"

namespace im {

/**
 * @brief 消息查询服务：历史、最近、搜索。
 */
class MessageServiceImpl final : public MessageService {
public:
    MessageServiceImpl(std::shared_ptr<odb::core::database> db,
                       std::shared_ptr<ESClient> es,
                       ServiceManager::Ptr channels,
                       std::string user_service);

    void GetHistory(google::protobuf::RpcController* controller,
                    const GetHistoryMsgReq* request,
                    GetHistoryMsgRsp* response,
                    google::protobuf::Closure* done) override;

    void GetRecent(google::protobuf::RpcController* controller,
                   const GetRecentMsgReq* request,
                   GetRecentMsgRsp* response,
                   google::protobuf::Closure* done) override;

    void Search(google::protobuf::RpcController* controller,
                const MsgSearchReq* request,
                MsgSearchRsp* response,
                google::protobuf::Closure* done) override;

private:
    /// 批量拉取发送者信息
    bool fetchUsers(const std::string& request_id,
                    const std::vector<std::string>& user_ids,
                    std::unordered_map<std::string, UserInfo>& users);

    /// 把数据库消息转成 MessageInfo
    void buildMessage(const Message& message,
                      const std::unordered_map<std::string, UserInfo>& users,
                      MessageInfo* info);

    std::shared_ptr<odb::core::database> db_;
    std::shared_ptr<MessageDao> messages_;
    std::shared_ptr<ESClient> es_;
    ServiceManager::Ptr channels_;
    std::string user_service_;
};

} // namespace im
