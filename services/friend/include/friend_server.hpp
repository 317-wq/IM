/**
 * @file friend_server.hpp
 * @brief 好友与会话服务实现类声明（实现见 services/friend/src/friend_server.cpp）。
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <odb/database.hxx>

#include "channel.hpp"
#include "db/chat_session_dao.hpp"
#include "db/chat_session_member_dao.hpp"
#include "db/friend_apply_dao.hpp"
#include "db/friend_repository.hpp"
#include "db/relation_dao.hpp"
#include "es.hpp"
#include "friend.pb.h"

namespace im {

/**
 * @brief 好友与会话服务实现。
 */
class FriendServiceImpl final : public FriendService {
public:
    FriendServiceImpl(std::shared_ptr<odb::core::database> db,
                      ServiceManager::Ptr channels,
                      std::shared_ptr<ESClient> es,
                      std::string user_service);

    void GetFriendList(google::protobuf::RpcController* controller,
                       const GetFriendListReq* request,
                       GetFriendListRsp* response,
                       google::protobuf::Closure* done) override;

    void FriendAdd(google::protobuf::RpcController* controller,
                   const FriendAddReq* request,
                   FriendAddRsp* response,
                   google::protobuf::Closure* done) override;

    void FriendAddProcess(google::protobuf::RpcController* controller,
                          const FriendAddProcessReq* request,
                          FriendAddProcessRsp* response,
                          google::protobuf::Closure* done) override;

    void GetPendingFriendEventList(google::protobuf::RpcController* controller,
                                   const GetPendingFriendEventListReq* request,
                                   GetPendingFriendEventListRsp* response,
                                   google::protobuf::Closure* done) override;

    void FriendRemove(google::protobuf::RpcController* controller,
                      const FriendRemoveReq* request,
                      FriendRemoveRsp* response,
                      google::protobuf::Closure* done) override;

    void FriendSearch(google::protobuf::RpcController* controller,
                      const FriendSearchReq* request,
                      FriendSearchRsp* response,
                      google::protobuf::Closure* done) override;

    void GetChatSessionList(google::protobuf::RpcController* controller,
                            const GetChatSessionListReq* request,
                            GetChatSessionListRsp* response,
                            google::protobuf::Closure* done) override;

    void ChatSessionCreate(google::protobuf::RpcController* controller,
                           const ChatSessionCreateReq* request,
                           ChatSessionCreateRsp* response,
                           google::protobuf::Closure* done) override;

    void GetChatSessionMember(google::protobuf::RpcController* controller,
                              const GetChatSessionMemberReq* request,
                              GetChatSessionMemberRsp* response,
                              google::protobuf::Closure* done) override;

private:
    bool fetchUsers(const std::string& request_id,
                    const std::vector<std::string>& user_ids,
                    std::vector<UserInfo>& users);

    std::shared_ptr<odb::core::database> db_;
    std::shared_ptr<RelationDao> relations_;
    std::shared_ptr<FriendApplyDao> applies_;
    std::shared_ptr<ChatSessionDao> sessions_;
    std::shared_ptr<ChatSessionMemberDao> members_;
    std::shared_ptr<FriendRepository> repository_;
    ServiceManager::Ptr channels_;
    std::shared_ptr<ESClient> es_;
    std::string user_service_;
};

} // namespace im
