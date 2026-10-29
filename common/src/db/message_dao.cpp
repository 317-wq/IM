/**
 * @file message_dao.cpp
 * @brief 消息 DAO 实现。
 */

#include "db/message_dao.hpp"

#include <odb/query.hxx>
#include <odb/transaction.hxx>

#include "logger.hpp"
#include "message-odb.hxx"

namespace im {

/**
 * @brief 实现构造。
 */
MessageDao::MessageDao(std::shared_ptr<odb::core::database> db)
    : db_(std::move(db)) {}

/**
 * @brief 实现插入。
 */
bool MessageDao::insert(Message& message) {
    try {
        odb::transaction trans(db_->begin());
        db_->persist(message);
        trans.commit();
        return true;
    } catch (const std::exception& e) {
        IM_ERROR("insert message {} failed: {}", message.message_id(), e.what());
        return false;
    }
}

/**
 * @brief 实现按会话查询全部消息。
 */
std::vector<Message> MessageDao::listBySession(const std::string& session_id) {
    std::vector<Message> result;
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<Message> query;
        odb::result<Message> rows(db_->query<Message>(query::session_id == session_id));
        for (const auto& row : rows) {
            result.push_back(row);
        }
        trans.commit();
    } catch (const std::exception& e) {
        IM_ERROR("list messages of {} failed: {}", session_id, e.what());
    }
    return result;
}

/**
 * @brief 实现按时间范围查询。
 */
std::vector<Message> MessageDao::range(const std::string& session_id,
                                       const boost::posix_time::ptime& start,
                                       const boost::posix_time::ptime& end) {
    std::vector<Message> result;
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<Message> query;
        odb::result<Message> rows(
            db_->query<Message>(query::session_id == session_id &&
                                query::created_at >= start && query::created_at <= end));
        for (const auto& row : rows) {
            result.push_back(row);
        }
        trans.commit();
    } catch (const std::exception& e) {
        IM_ERROR("range messages of {} failed: {}", session_id, e.what());
    }
    return result;
}

} // namespace im
