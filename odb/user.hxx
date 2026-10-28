/**
 * @file user.hxx
 * @brief user 表的 ODB 实体声明。
 *
 * 字段与 sql/user.sql 对应；密码只存哈希和盐，不存明文。
 * user_id、nickname 唯一，create_time 使用 boost::posix_time::ptime 映射到 TIMESTAMP。
 */

#pragma once

#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <odb/core.hxx>
#include <odb/nullable.hxx>

namespace im {

/**
 * @brief 用户实体。
 */
#pragma db object table("user")
class User {
public:
    User() = default;

    /**
     * @brief 构造一个新用户；创建时间取当前本地时间。
     */
    User(std::string user_id, std::string nickname, std::string password_hash,
         std::string salt)
        : _user_id(std::move(user_id)),
          _nickname(std::move(nickname)),
          _password_hash(std::move(password_hash)),
          _salt(std::move(salt)),
          _created_at(boost::posix_time::second_clock::local_time()) {}

    /// 数据库自增主键
    unsigned long id() const { return _id; }

    /// 业务用户 ID（uuid）
    const std::string& user_id() const { return _user_id; }
    void user_id(const std::string& value) { _user_id = value; }

    /// 昵称（唯一）
    const std::string& nickname() const { return _nickname; }
    void nickname(const std::string& value) { _nickname = value; }

    /// 密码哈希（salt + password 的 sha256）
    const std::string& password_hash() const { return _password_hash; }
    void password_hash(const std::string& value) { _password_hash = value; }

    /// 密码盐
    const std::string& salt() const { return _salt; }
    void salt(const std::string& value) { _salt = value; }

    /// 个人签名，可为空
    std::string description() const {
        return _description ? *_description : std::string();
    }
    void description(const std::string& value) { _description = value; }

    /// 头像文件 ID，可为空
    std::string avatar_id() const {
        return _avatar_id ? *_avatar_id : std::string();
    }
    void avatar_id(const std::string& value) { _avatar_id = value; }

    /// 创建时间
    const boost::posix_time::ptime& created_at() const { return _created_at; }
    void created_at(const boost::posix_time::ptime& value) { _created_at = value; }

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long _id = 0;

    #pragma db type("varchar(64)") index unique
    std::string _user_id;

    #pragma db type("varchar(64)") index unique
    std::string _nickname;

    #pragma db type("varchar(128)")
    std::string _password_hash;

    #pragma db type("varchar(32)")
    std::string _salt;

    #pragma db type("TEXT")
    odb::nullable<std::string> _description;

    #pragma db type("varchar(64)")
    odb::nullable<std::string> _avatar_id;

    #pragma db type("TIMESTAMP")
    boost::posix_time::ptime _created_at;
};

} // namespace im
