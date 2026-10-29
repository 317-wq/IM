/**
 * @file friend_apply.hxx
 * @brief 好友申请实体：处理完（同意/拒绝）后删除。
 */

#pragma once

#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <odb/core.hxx>

namespace im {

#pragma db object table("friend_apply")
class FriendApply {
public:
    FriendApply() = default;

    FriendApply(std::string event_id, std::string from_user, std::string to_user)
        : _event_id(std::move(event_id)),
          _from_user(std::move(from_user)),
          _to_user(std::move(to_user)),
          _created_at(boost::posix_time::second_clock::local_time()) {}

    const std::string& event_id() const { return _event_id; }
    const std::string& from_user() const { return _from_user; }
    const std::string& to_user() const { return _to_user; }
    const boost::posix_time::ptime& created_at() const { return _created_at; }

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long _id = 0;

    #pragma db type("varchar(64)") index unique
    std::string _event_id;

    #pragma db type("varchar(64)") index
    std::string _from_user;

    #pragma db type("varchar(64)") index
    std::string _to_user;

    #pragma db type("TIMESTAMP")
    boost::posix_time::ptime _created_at;
};

} // namespace im
