/**
 * @file message.hxx
 * @brief 消息实体：文本/图片/文件/语音消息统一落这张表。
 */

#pragma once

#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <odb/core.hxx>
#include <odb/nullable.hxx>

namespace im {

#pragma db object table("message")
class Message {
public:
    Message() = default;

    Message(std::string message_id, std::string session_id, std::string sender_id,
            unsigned char type, const boost::posix_time::ptime& created_at)
        : _message_id(std::move(message_id)),
          _session_id(std::move(session_id)),
          _sender_id(std::move(sender_id)),
          _type(type),
          _created_at(created_at) {}

    const std::string& message_id() const { return _message_id; }
    const std::string& session_id() const { return _session_id; }
    const std::string& sender_id() const { return _sender_id; }
    unsigned char type() const { return _type; }
    const boost::posix_time::ptime& created_at() const { return _created_at; }

    std::string content() const { return _content ? *_content : std::string(); }
    void content(const std::string& value) { _content = value; }

    std::string file_id() const { return _file_id ? *_file_id : std::string(); }
    void file_id(const std::string& value) { _file_id = value; }

    std::string file_name() const { return _file_name ? *_file_name : std::string(); }
    void file_name(const std::string& value) { _file_name = value; }

    unsigned int file_size() const { return _file_size ? *_file_size : 0; }
    void file_size(unsigned int value) { _file_size = value; }

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long _id = 0;

    #pragma db type("varchar(64)") index unique
    std::string _message_id;

    #pragma db type("varchar(64)") index
    std::string _session_id;

    #pragma db type("varchar(64)")
    std::string _sender_id;

    unsigned char _type = 0;

    #pragma db type("TEXT")
    odb::nullable<std::string> _content;

    #pragma db type("varchar(64)")
    odb::nullable<std::string> _file_id;

    #pragma db type("varchar(128)")
    odb::nullable<std::string> _file_name;

    odb::nullable<unsigned int> _file_size;

    #pragma db type("TIMESTAMP")
    boost::posix_time::ptime _created_at;
};

} // namespace im
