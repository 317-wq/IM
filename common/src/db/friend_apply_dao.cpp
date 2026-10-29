/**
 * @file friend_apply_dao.cpp
 * @brief 好友申请 DAO 实现。
 */

#include "db/friend_apply_dao.hpp"

#include <odb/query.hxx>
#include <odb/transaction.hxx>

#include "friend_apply-odb.hxx"
#include "logger.hpp"

namespace im {

/**
 * @brief 实现构造。
 */
FriendApplyDao::FriendApplyDao(std::shared_ptr<odb::core::database> db)
    : db_(std::move(db)) {}

/**
 * @brief 实现插入申请。
 */
bool FriendApplyDao::insert(FriendApply& apply) {
    try {
        odb::transaction trans(db_->begin());
        db_->persist(apply);
        trans.commit();
        return true;
    } catch (const std::exception& e) {
        IM_ERROR("insert apply {} -> {} failed: {}", apply.from_user(), apply.to_user(),
                 e.what());
        return false;
    }
}

/**
 * @brief 实现申请存在性判断。
 */
bool FriendApplyDao::exists(const std::string& from_user, const std::string& to_user) {
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<FriendApply> query;
        std::shared_ptr<FriendApply> apply(
            db_->query_one<FriendApply>(query::from_user == from_user &&
                                        query::to_user == to_user));
        trans.commit();
        return apply != nullptr;
    } catch (const std::exception& e) {
        IM_ERROR("query apply {} -> {} failed: {}", from_user, to_user, e.what());
        return false;
    }
}

/**
 * @brief 实现申请删除。
 */
bool FriendApplyDao::remove(const std::string& from_user, const std::string& to_user) {
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<FriendApply> query;
        db_->erase_query<FriendApply>(query::from_user == from_user &&
                                      query::to_user == to_user);
        trans.commit();
        return true;
    } catch (const std::exception& e) {
        IM_ERROR("remove apply {} -> {} failed: {}", from_user, to_user, e.what());
        return false;
    }
}

/**
 * @brief 实现列出某人收到的申请。
 */
std::vector<FriendApply> FriendApplyDao::listByRespondent(const std::string& to_user) {
    std::vector<FriendApply> result;
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<FriendApply> query;
        odb::result<FriendApply> rows(
            db_->query<FriendApply>(query::to_user == to_user));
        for (const auto& row : rows) {
            result.push_back(row);
        }
        trans.commit();
    } catch (const std::exception& e) {
        IM_ERROR("list applies of {} failed: {}", to_user, e.what());
    }
    return result;
}

} // namespace im
