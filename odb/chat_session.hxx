/**
 * @file chat_session.hxx
 * @brief 会话实体：单聊（SINGLE）与群聊（GROUP）。
 */

#pragma once

#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <odb/core.hxx>

namespace im {

/// 会话类型
enum class ChatSessionType : unsigned char {
    SINGLE = 0,  ///< 单聊
    GROUP = 1    ///< 群聊
};

#pragma db object table("chat_session")
class ChatSession {
public:
    ChatSession() = default;

    ChatSession(std::string session_id, std::string name, ChatSessionType type)
        : _session_id(std::move(session_id)),
          _name(std::move(name)),
          _type(static_cast<unsigned char>(type)),
          _created_at(boost::posix_time::second_clock::local_time()) {}

    const std::string& session_id() const { return _session_id; }
    const std::string& name() const { return _name; }
    ChatSessionType type() const { return static_cast<ChatSessionType>(_type); }
    const boost::posix_time::ptime& created_at() const { return _created_at; }

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long _id = 0;

    #pragma db type("varchar(64)") index unique
    std::string _session_id;

    #pragma db type("varchar(64)")
    std::string _name;

    unsigned char _type = 0;

    #pragma db type("TIMESTAMP")
    boost::posix_time::ptime _created_at;
};

} // namespace im
