/**
 * @file relation_dao.hpp
 * @brief 好友关系 DAO 声明（实现见 common/src/db/relation_dao.cpp）。
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <odb/database.hxx>

namespace im {

/**
 * @brief 好友关系表访问对象：关系双向存两条。
 */
class RelationDao {
public:
    explicit RelationDao(std::shared_ptr<odb::core::database> db);

    /// 插入双向关系（两条记录，一个事务）
    bool insertPair(const std::string& user_id, const std::string& peer_id);

    /// 删除双向关系
    bool removePair(const std::string& user_id, const std::string& peer_id);

    /// 判断两人是否已经是好友
    bool exists(const std::string& user_id, const std::string& peer_id);

    /// 列出某用户的所有好友 user_id
    std::vector<std::string> friends(const std::string& user_id);

private:
    std::shared_ptr<odb::core::database> db_;
};

} // namespace im
