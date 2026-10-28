/**
 * @file user_dao.cpp
 * @brief user 表 DAO 实现：每个方法独立开事务，异常统一转成失败返回。
 */

#include "db/user_dao.hpp"

#include <odb/query.hxx>
#include <odb/transaction.hxx>

#include "logger.hpp"
#include "user-odb.hxx"

namespace im {

/**
 * @brief 实现构造：保存数据库对象。
 */
UserDao::UserDao(std::shared_ptr<odb::core::database> db) : db_(std::move(db)) {}

/**
 * @brief 实现插入：事务内 persist，异常记录日志并返回 false。
 */
bool UserDao::insert(User& user) {
    try {
        odb::transaction trans(db_->begin());
        db_->persist(user);
        trans.commit();
        return true;
    } catch (const std::exception& e) {
        IM_ERROR("insert user {} failed: {}", user.nickname(), e.what());
        return false;
    }
}

/**
 * @brief 实现更新：事务内 update。
 */
bool UserDao::update(const User& user) {
    try {
        odb::transaction trans(db_->begin());
        db_->update(user);
        trans.commit();
        return true;
    } catch (const std::exception& e) {
        IM_ERROR("update user {} failed: {}", user.user_id(), e.what());
        return false;
    }
}

/**
 * @brief 实现按 user_id 查询。
 */
std::shared_ptr<User> UserDao::findByUserId(const std::string& user_id) {
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<User> query;
        std::shared_ptr<User> user(db_->query_one<User>(query::user_id == user_id));
        trans.commit();
        return user;
    } catch (const std::exception& e) {
        IM_ERROR("query user by id {} failed: {}", user_id, e.what());
        return nullptr;
    }
}

/**
 * @brief 实现按昵称查询。
 */
std::shared_ptr<User> UserDao::findByNickname(const std::string& nickname) {
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<User> query;
        std::shared_ptr<User> user(db_->query_one<User>(query::nickname == nickname));
        trans.commit();
        return user;
    } catch (const std::exception& e) {
        IM_ERROR("query user by nickname {} failed: {}", nickname, e.what());
        return nullptr;
    }
}

} // namespace im
