/**
 * @file message_dao.hpp
 * @brief 消息 DAO 声明（实现见 common/src/db/message_dao.cpp）。
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <odb/database.hxx>

#include "message.hxx"

namespace im {

/**
 * @brief 消息表访问对象。
 */
class MessageDao {
public:
    explicit MessageDao(std::shared_ptr<odb::core::database> db);

    /// 插入一条消息
    bool insert(Message& message);

    /// 查询会话全部消息
    std::vector<Message> listBySession(const std::string& session_id);

    /// 按时间范围查询会话消息
    std::vector<Message> range(const std::string& session_id,
                               const boost::posix_time::ptime& start,
                               const boost::posix_time::ptime& end);

private:
    std::shared_ptr<odb::core::database> db_;
};

} // namespace im
