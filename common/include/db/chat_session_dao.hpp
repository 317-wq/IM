/**
 * @file chat_session_dao.hpp
 * @brief 会话 DAO 声明（实现见 common/src/db/chat_session_dao.cpp）。
 */

#pragma once

#include <memory>
#include <string>

#include <odb/database.hxx>

#include "chat_session.hxx"

namespace im {

/**
 * @brief 会话表访问对象。
 */
class ChatSessionDao {
public:
    explicit ChatSessionDao(std::shared_ptr<odb::core::database> db);

    /// 插入会话
    bool insert(ChatSession& session);

    /// 按 session_id 查询；不存在返回空指针
    std::shared_ptr<ChatSession> findById(const std::string& session_id);

private:
    std::shared_ptr<odb::core::database> db_;
};

} // namespace im
