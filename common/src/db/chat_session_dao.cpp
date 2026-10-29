/**
 * @file chat_session_dao.cpp
 * @brief 会话 DAO 实现。
 */

#include "db/chat_session_dao.hpp"

#include <odb/query.hxx>
#include <odb/transaction.hxx>

#include "chat_session-odb.hxx"
#include "logger.hpp"

namespace im {

/**
 * @brief 实现构造。
 */
ChatSessionDao::ChatSessionDao(std::shared_ptr<odb::core::database> db)
    : db_(std::move(db)) {}

/**
 * @brief 实现插入会话。
 */
bool ChatSessionDao::insert(ChatSession& session) {
    try {
        odb::transaction trans(db_->begin());
        db_->persist(session);
        trans.commit();
        return true;
    } catch (const std::exception& e) {
        IM_ERROR("insert session {} failed: {}", session.session_id(), e.what());
        return false;
    }
}

/**
 * @brief 实现按 session_id 查询。
 */
std::shared_ptr<ChatSession> ChatSessionDao::findById(const std::string& session_id) {
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<ChatSession> query;
        std::shared_ptr<ChatSession> session(
            db_->query_one<ChatSession>(query::session_id == session_id));
        trans.commit();
        return session;
    } catch (const std::exception& e) {
        IM_ERROR("query session {} failed: {}", session_id, e.what());
        return nullptr;
    }
}

} // namespace im
