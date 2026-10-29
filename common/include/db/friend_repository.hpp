/**
 * @file friend_repository.hpp
 * @brief 好友/会话的跨表事务操作声明（实现见 common/src/db/friend_repository.cpp）。
 *
 * 单表读写走各自的 DAO；需要“多表一起成功/失败”的场景放在这里，
 * 保证同意好友、建群这类操作在一个数据库事务内完成。
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <odb/database.hxx>

namespace im {

/**
 * @brief 好友与会话的跨表事务仓库。
 */
class FriendRepository {
public:
    explicit FriendRepository(std::shared_ptr<odb::core::database> db);

    /**
     * @brief 同意好友申请。
     *
     * 一个事务内完成：删除申请、写入双向关系、创建单聊会话、写入两个成员。
     * @param from_user  申请人 user_id
     * @param to_user    被申请人 user_id
     * @param session_id 新会话 ID
     */
    bool acceptFriend(const std::string& from_user,
                      const std::string& to_user,
                      const std::string& session_id);

    /**
     * @brief 创建群聊会话。
     *
     * 一个事务内完成：创建会话、写入所有成员。
     * @param session_id 新会话 ID
     * @param name       群名称
     * @param member_ids 成员 user_id 列表（包含创建者）
     */
    bool createGroup(const std::string& session_id,
                     const std::string& name,
                     const std::vector<std::string>& member_ids);

private:
    std::shared_ptr<odb::core::database> db_;
};

} // namespace im
