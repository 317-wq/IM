/**
 * @file relation.hxx
 * @brief 好友关系实体：双向各存一条，user_id 与 peer_id 都是业务 user_id。
 */

#pragma once

#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <odb/core.hxx>

namespace im {

#pragma db object table("relation")
class Relation {
public:
    Relation() = default;

    Relation(std::string user_id, std::string peer_id)
        : _user_id(std::move(user_id)),
          _peer_id(std::move(peer_id)),
          _created_at(boost::posix_time::second_clock::local_time()) {}

    const std::string& user_id() const { return _user_id; }
    const std::string& peer_id() const { return _peer_id; }
    const boost::posix_time::ptime& created_at() const { return _created_at; }

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long _id = 0;

    #pragma db type("varchar(64)") index
    std::string _user_id;

    #pragma db type("varchar(64)") index
    std::string _peer_id;

    #pragma db type("TIMESTAMP")
    boost::posix_time::ptime _created_at;
};

} // namespace im
