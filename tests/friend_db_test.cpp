/**
 * @file friend_db_test.cpp
 * @brief M4 数据层验收：好友关系、好友申请、会话、会话成员与跨表事务。
 */

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <gflags/gflags.h>

#include "db/chat_session_dao.hpp"
#include "db/chat_session_member_dao.hpp"
#include "db/friend_apply_dao.hpp"
#include "db/friend_repository.hpp"
#include "db/relation_dao.hpp"
#include "logger.hpp"
#include "mysql.hpp"
#include "utils.hpp"

DEFINE_string(mysql_host, "127.0.0.1", "MySQL 地址");
DEFINE_int32(mysql_port, 3306, "MySQL 端口");
DEFINE_string(mysql_user, "root", "MySQL 用户名");
DEFINE_string(mysql_password, "", "MySQL 密码；为空时读 IM_MYSQL_PASSWORD");
DEFINE_string(mysql_db, "im", "MySQL 数据库名");
DEFINE_string(mysql_charset, "utf8mb4", "MySQL 字符集");
DEFINE_int32(mysql_pool, 4, "连接池大小");

DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/friend_db_test", "发布模式下的日志目录");
DEFINE_int32(log_level, 0, "日志等级");

namespace {

int failures = 0;

/**
 * @brief 断言辅助：打印结果并累计失败数。
 */
void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", what.c_str());
    if (!ok) {
        ++failures;
    }
}

} // namespace

/**
 * @brief 测试入口：关系 -> 申请 -> 会话成员 -> 同意好友的跨表事务。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    im::init_logger(FLAGS_run_mode, FLAGS_log_dir, FLAGS_log_level);

    std::string password = FLAGS_mysql_password;
    if (password.empty()) {
        const char* env = std::getenv("IM_MYSQL_PASSWORD");
        if (env != nullptr) {
            password = env;
        }
    }

    auto db = im::ODBFactory::create(FLAGS_mysql_user, password, FLAGS_mysql_host,
                                     FLAGS_mysql_db, FLAGS_mysql_charset,
                                     static_cast<unsigned int>(FLAGS_mysql_port),
                                     FLAGS_mysql_pool);

    im::RelationDao relations(db);
    im::FriendApplyDao applies(db);
    im::ChatSessionDao sessions(db);
    im::ChatSessionMemberDao members(db);
    im::FriendRepository repository(db);

    const std::string alice = im::uuid();
    const std::string bob = im::uuid();

    // 关系
    check(relations.insertPair(alice, bob), "插入双向好友关系");
    check(relations.exists(alice, bob) && relations.exists(bob, alice), "双向关系都存在");
    auto alice_friends = relations.friends(alice);
    check(alice_friends.size() == 1 && alice_friends[0] == bob, "好友列表正确");
    check(relations.removePair(alice, bob), "删除双向关系");
    check(!relations.exists(alice, bob) && !relations.exists(bob, alice), "关系已删除");

    // 申请
    im::FriendApply apply(im::uuid(), alice, bob);
    check(applies.insert(apply), "插入好友申请");
    check(applies.exists(alice, bob), "申请存在");
    auto pending = applies.listByRespondent(bob);
    check(pending.size() == 1 && pending[0].from_user() == alice, "收件申请列表正确");
    check(applies.remove(alice, bob), "删除申请");
    check(!applies.exists(alice, bob), "申请已删除");

    // 会话与成员
    im::ChatSession group(im::uuid(), "test-group", im::ChatSessionType::GROUP);
    check(sessions.insert(group), "插入群聊会话");
    std::vector<im::ChatSessionMember> group_members = {
        im::ChatSessionMember(group.session_id(), alice),
        im::ChatSessionMember(group.session_id(), bob),
    };
    check(members.append(group_members), "写入群聊成员");
    check(members.memberIds(group.session_id()).size() == 2, "群成员数量为 2");
    check(members.sessionIds(alice).size() == 1, "用户会话列表包含群聊");

    // 同意好友：一个事务内删申请、写关系、建单聊、写成员
    const std::string apply_id = im::uuid();
    const std::string single_id = im::uuid();
    im::FriendApply pending_apply(apply_id, alice, bob);
    check(applies.insert(pending_apply), "插入待处理申请");
    check(repository.acceptFriend(alice, bob, single_id), "同意好友事务执行成功");
    check(relations.exists(alice, bob) && relations.exists(bob, alice), "同意后双向关系存在");
    check(!applies.exists(alice, bob), "同意后申请被删除");
    auto single = sessions.findById(single_id);
    check(single != nullptr && single->type() == im::ChatSessionType::SINGLE,
          "生成单聊会话");
    check(members.memberIds(single_id).size() == 2, "单聊包含两个成员");

    std::printf("%s\n", failures == 0 ? "ALL PASS" : "SOME FAILED");
    return failures == 0 ? 0 : 1;
}
