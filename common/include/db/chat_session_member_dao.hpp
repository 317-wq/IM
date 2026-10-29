/**
 * @file chat_session_member_dao.hpp
 * @brief 会话成员 DAO 声明（实现见 common/src/db/chat_session_member_dao.cpp）。
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <odb/database.hxx>

#include "chat_session_member.hxx"

namespace im {

/**
 * @brief 会话成员表访问对象。
 */
class ChatSessionMemberDao {
public:
    explicit ChatSessionMemberDao(std::shared_ptr<odb::core::database> db);

    /// 批量追加成员（一个事务）
    bool append(const std::vector<ChatSessionMember>& members);

    /// 列出某用户加入的所有会话 ID
    std::vector<std::string> sessionIds(const std::string& user_id);

    /// 列出某个会话的所有成员 user_id
    std::vector<std::string> memberIds(const std::string& session_id);

private:
    std::shared_ptr<odb::core::database> db_;
};

} // namespace im
