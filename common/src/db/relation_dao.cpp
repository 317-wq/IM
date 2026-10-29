/**
 * @file relation_dao.cpp
 * @brief 好友关系 DAO 实现。
 */

#include "db/relation_dao.hpp"

#include <odb/query.hxx>
#include <odb/transaction.hxx>

#include "logger.hpp"
#include "relation-odb.hxx"
#include "relation.hxx"

namespace im {

/**
 * @brief 实现构造。
 */
RelationDao::RelationDao(std::shared_ptr<odb::core::database> db)
    : db_(std::move(db)) {}

/**
 * @brief 实现双向插入：两条关系在同一个事务里写入。
 */
bool RelationDao::insertPair(const std::string& user_id, const std::string& peer_id) {
    try {
        odb::transaction trans(db_->begin());
        Relation forward(user_id, peer_id);
        Relation backward(peer_id, user_id);
        db_->persist(forward);
        db_->persist(backward);
        trans.commit();
        return true;
    } catch (const std::exception& e) {
        IM_ERROR("insert relation {}-{} failed: {}", user_id, peer_id, e.what());
        return false;
    }
}

/**
 * @brief 实现双向删除。
 */
bool RelationDao::removePair(const std::string& user_id, const std::string& peer_id) {
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<Relation> query;
        db_->erase_query<Relation>(query::user_id == user_id && query::peer_id == peer_id);
        db_->erase_query<Relation>(query::user_id == peer_id && query::peer_id == user_id);
        trans.commit();
        return true;
    } catch (const std::exception& e) {
        IM_ERROR("remove relation {}-{} failed: {}", user_id, peer_id, e.what());
        return false;
    }
}

/**
 * @brief 实现好友存在性判断。
 */
bool RelationDao::exists(const std::string& user_id, const std::string& peer_id) {
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<Relation> query;
        std::shared_ptr<Relation> relation(
            db_->query_one<Relation>(query::user_id == user_id && query::peer_id == peer_id));
        trans.commit();
        return relation != nullptr;
    } catch (const std::exception& e) {
        IM_ERROR("query relation {}-{} failed: {}", user_id, peer_id, e.what());
        return false;
    }
}

/**
 * @brief 实现好友列表查询。
 */
std::vector<std::string> RelationDao::friends(const std::string& user_id) {
    std::vector<std::string> result;
    try {
        odb::transaction trans(db_->begin());
        typedef odb::query<Relation> query;
        odb::result<Relation> rows(db_->query<Relation>(query::user_id == user_id));
        for (const auto& row : rows) {
            result.push_back(row.peer_id());
        }
        trans.commit();
    } catch (const std::exception& e) {
        IM_ERROR("list friends of {} failed: {}", user_id, e.what());
    }
    return result;
}

} // namespace im
