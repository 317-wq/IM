/**
 * @file friend_apply_dao.hpp
 * @brief 好友申请 DAO 声明（实现见 common/src/db/friend_apply_dao.cpp）。
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <odb/database.hxx>

#include "friend_apply.hxx"

namespace im {

/**
 * @brief 好友申请表访问对象。
 */
class FriendApplyDao {
public:
    explicit FriendApplyDao(std::shared_ptr<odb::core::database> db);

    /// 插入申请
    bool insert(FriendApply& apply);

    /// 判断 from -> to 是否已有未处理申请
    bool exists(const std::string& from_user, const std::string& to_user);

    /// 删除 from -> to 的申请
    bool remove(const std::string& from_user, const std::string& to_user);

    /// 列出 to_user 收到的所有申请
    std::vector<FriendApply> listByRespondent(const std::string& to_user);

private:
    std::shared_ptr<odb::core::database> db_;
};

} // namespace im
