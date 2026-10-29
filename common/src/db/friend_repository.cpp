/**
 * @file friend_repository.cpp
 * @brief 好友/会话跨表事务实现。
 */

#include "db/friend_repository.hpp"

#include <odb/query.hxx>
#include <odb/transaction.hxx>

#include "chat_session-odb.hxx"
#include "chat_session_member-odb.hxx"
#include "friend_apply-odb.hxx"
#include "logger.hpp"
#include "relation-odb.hxx"

namespace im {

/**
 * @brief 实现构造。
 */
FriendRepository::FriendRepository(std::shared_ptr<odb::core::database> db)
    : db_(std::move(db)) {}

/**
 * @brief 实现同意好友：同一事务内删除申请、双向关系、单聊会话、两个成员。
 */
bool FriendRepository::acceptFriend(const std::string& from_user,
                                    const std::string& to_user,
                                    const std::string& session_id) {
    try {
        odb::transaction trans(db_->begin());

        typedef odb::query<FriendApply> apply_query;
        db_->erase_query<FriendApply>(apply_query::from_user == from_user &&
                                      apply_query::to_user == to_user);

        Relation forward(from_user, to_user);
        Relation backward(to_user, from_user);
        db_->persist(forward);
        db_->persist(backward);

        ChatSession session(session_id, "", ChatSessionType::SINGLE);
        db_->persist(session);
        ChatSessionMember from_member(session_id, from_user);
        ChatSessionMember to_member(session_id, to_user);
        db_->persist(from_member);
        db_->persist(to_member);

        trans.commit();
        return true;
    } catch (const std::exception& e) {
        IM_ERROR("accept friend {} -> {} failed: {}", from_user, to_user, e.what());
        return false;
    }
}

/**
 * @brief 实现建群：同一事务内创建会话并写入所有成员。
 */
bool FriendRepository::createGroup(const std::string& session_id,
                                   const std::string& name,
                                   const std::vector<std::string>& member_ids) {
    try {
        odb::transaction trans(db_->begin());

        ChatSession session(session_id, name, ChatSessionType::GROUP);
        db_->persist(session);
        for (const auto& user_id : member_ids) {
            ChatSessionMember member(session_id, user_id);
            db_->persist(member);
        }

        trans.commit();
        return true;
    } catch (const std::exception& e) {
        IM_ERROR("create group {} failed: {}", session_id, e.what());
        return false;
    }
}

} // namespace im
