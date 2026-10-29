/**
 * @file chat_session_member_dao.cpp
 * @brief 会话成员 DAO 实现。
 */

#include "db/chat_session_member_dao.hpp"

#include <odb/query.hxx>
#include <odb/transaction.hxx>

#include "chat_session_member-odb.hxx"
#include "logger.hpp"

namespace im {

/**
 * @brief 实现构造。
 */
ChatSessionMemberDao::ChatSessionMemberDao(std::shared_ptr<odb::core::database> db)
    : db_(std::move(db)) {}

/**
 * @brief 实现批量追加成员。
 */
bool ChatSessionMemberDao::append(const std::vector<ChatSessionMember>& members) {
    try {
        odb::transaction trans(db_->begin());
        for (const auto& member : members) {
            ChatSessionMember copy = member;
            db_->persist(copy);
        }
        trans.commit();
        return true;
    } catch (const std::exception& e) {
        IM_ERROR("append session members failed: {}", e.what());
        return false;
    }
}

/**
 * @brief 实现查询用户加入的会话 ID 列表。
 */
std::vector<std::string> ChatSessionMemberDao::sessionIds(const std::string& user_id) {
    std::vector<std::string> result;
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<ChatSessionMember> query;
        odb::result<ChatSessionMember> rows(
            db_->query<ChatSessionMember>(query::user_id == user_id));
        for (const auto& row : rows) {
            result.push_back(row.session_id());
        }
        trans.commit();
    } catch (const std::exception& e) {
        IM_ERROR("list sessions of {} failed: {}", user_id, e.what());
    }
    return result;
}

/**
 * @brief 实现查询会话成员 ID 列表。
 */
std::vector<std::string> ChatSessionMemberDao::memberIds(const std::string& session_id) {
    std::vector<std::string> result;
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<ChatSessionMember> query;
        odb::result<ChatSessionMember> rows(
            db_->query<ChatSessionMember>(query::session_id == session_id));
        for (const auto& row : rows) {
            result.push_back(row.user_id());
        }
        trans.commit();
    } catch (const std::exception& e) {
        IM_ERROR("list members of {} failed: {}", session_id, e.what());
    }
    return result;
}

} // namespace im
