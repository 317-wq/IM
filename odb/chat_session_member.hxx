/**
 * @file chat_session_member.hxx
 * @brief 会话成员实体：一个会话下每个用户一条。
 */

#pragma once

#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <odb/core.hxx>

namespace im {

#pragma db object table("chat_session_member")
class ChatSessionMember {
public:
    ChatSessionMember() = default;

    ChatSessionMember(std::string session_id, std::string user_id)
        : _session_id(std::move(session_id)),
          _user_id(std::move(user_id)),
          _joined_at(boost::posix_time::second_clock::local_time()) {}

    const std::string& session_id() const { return _session_id; }
    const std::string& user_id() const { return _user_id; }
    const boost::posix_time::ptime& joined_at() const { return _joined_at; }

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long _id = 0;

    #pragma db type("varchar(64)") index
    std::string _session_id;

    #pragma db type("varchar(64)") index
    std::string _user_id;

    #pragma db type("TIMESTAMP")
    boost::posix_time::ptime _joined_at;
};

} // namespace im
