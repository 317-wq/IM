/**
 * @file user_dao.hpp
 * @brief user 表的数据访问对象声明（实现见 common/src/db/user_dao.cpp）。
 *
 * 对外只暴露业务语义的方法，事务、ODB 查询细节都封装在实现里。
 */

#pragma once

#include <memory>
#include <string>

#include <odb/database.hxx>

#include "user.hxx"

namespace im {

/**
 * @brief user 表的 DAO。
 */
class UserDao {
public:
    /**
     * @brief 绑定一个数据库对象（通常来自 ODBFactory）。
     */
    explicit UserDao(std::shared_ptr<odb::core::database> db);

    /**
     * @brief 插入新用户；成功后 user.id() 会被填上自增主键。
     * @return 成功 true；唯一键冲突等异常返回 false
     */
    bool insert(User& user);

    /**
     * @brief 按主键更新用户全部字段。
     */
    bool update(const User& user);

    /**
     * @brief 按业务 user_id 查询；不存在返回空指针。
     */
    std::shared_ptr<User> findByUserId(const std::string& user_id);

    /**
     * @brief 按昵称查询；不存在返回空指针。
     */
    std::shared_ptr<User> findByNickname(const std::string& nickname);

private:
    std::shared_ptr<odb::core::database> db_;
};

} // namespace im
