/**
 * @file db_user_test.cpp
 * @brief M2 验收程序：验证 user 实体的插入、查询、更新与唯一约束。
 *
 * 运行前需要先执行 sql/user.sql 建库建表；密码可通过 -mysql_password
 * 或环境变量 IM_MYSQL_PASSWORD 传入（避免写进代码/配置）。
 */

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include <gflags/gflags.h>

#include "db/user_dao.hpp"
#include "logger.hpp"
#include "mysql.hpp"
#include "user.hxx"
#include "utils.hpp"

DEFINE_string(mysql_host, "127.0.0.1", "MySQL 地址");
DEFINE_int32(mysql_port, 3306, "MySQL 端口");
DEFINE_string(mysql_user, "root", "MySQL 用户名");
DEFINE_string(mysql_password, "", "MySQL 密码；为空时读 IM_MYSQL_PASSWORD");
DEFINE_string(mysql_db, "im", "MySQL 数据库名");
DEFINE_string(mysql_charset, "utf8mb4", "MySQL 字符集");
DEFINE_int32(mysql_pool, 4, "连接池大小");

DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/db_user_test", "发布模式下的日志目录");
DEFINE_int32(log_level, 0, "日志等级");

namespace {

int failures = 0;

/**
 * @brief 断言辅助：打印 [PASS]/[FAIL] 并累计失败数。
 */
void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", what.c_str());
    if (!ok) {
        ++failures;
    }
}

} // namespace

/**
 * @brief 测试入口：插入 -> 两种查询 -> 更新 -> 重复昵称插入失败。
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
    im::UserDao dao(db);

    const std::string nickname = "test_" + im::uuid().substr(0, 8);
    const std::string user_id = im::uuid();

    im::User user(user_id, nickname, "hash-placeholder", "salt-placeholder");
    check(dao.insert(user), "插入新用户");
    check(user.id() != 0, "插入后主键已回填");

    auto by_nickname = dao.findByNickname(nickname);
    check(by_nickname != nullptr, "按昵称查询到用户");
    if (by_nickname) {
        check(by_nickname->user_id() == user_id, "昵称查询的 user_id 一致");
        check(by_nickname->password_hash() == "hash-placeholder", "密码哈希一致");
    }

    auto by_id = dao.findByUserId(user_id);
    check(by_id != nullptr, "按 user_id 查询到用户");

    if (by_id) {
        by_id->description("hello im");
        check(dao.update(*by_id), "更新用户签名");
        auto updated = dao.findByUserId(user_id);
        check(updated != nullptr && updated->description() == "hello im", "签名更新生效");
    }

    im::User duplicate(im::uuid(), nickname, "hash2", "salt2");
    check(!dao.insert(duplicate), "重复昵称插入被拒绝");

    std::printf("%s\n", failures == 0 ? "ALL PASS" : "SOME FAILED");
    return failures == 0 ? 0 : 1;
}
