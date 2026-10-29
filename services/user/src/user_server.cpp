/**
 * @file user_server.cpp
 * @brief 用户服务实现与程序入口。
 *
 * 职责：
 *   1. 注册：account 查重、密码加盐哈希、写 MySQL；
 *   2. 登录：校验密码、单点登录检查、写 Redis 会话；
 *   3. 用户信息读写；头像通过 file 服务存取。
 */

#include "user_server.hpp"

#include <cctype>
#include <chrono>
#include <cstdlib>

#include <brpc/server.h>
#include <gflags/gflags.h>

#include "crypto.hpp"
#include "es.hpp"
#include "etcd.hpp"
#include "file.pb.h"
#include "logger.hpp"
#include "mysql.hpp"
#include "utils.hpp"

DEFINE_bool(run_mode, false, "运行模式：false-调试（控制台），true-发布（文件）");
DEFINE_string(log_dir, "logs/user", "发布模式下的日志目录");
DEFINE_int32(log_level, 0, "日志等级");

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd 地址");
DEFINE_string(service_base, "/im/service", "服务注册根目录");
DEFINE_string(service_name, "user", "本服务名称");
DEFINE_string(instance_id, "", "实例标识，空则使用 advertise_host");
DEFINE_string(advertise_host, "127.0.0.1:10002", "对外访问地址 host:port");
DEFINE_int32(listen_port, 10002, "RPC 监听端口");
DEFINE_int32(rpc_threads, 4, "RPC IO 线程数");
DEFINE_string(file_service, "/im/service/file", "文件服务名");
DEFINE_string(es_host, "http://127.0.0.1:9200", "Elasticsearch 地址");
DEFINE_string(es_index, "im_user", "用户搜索索引名");

DEFINE_string(mysql_host, "127.0.0.1", "MySQL 地址");
DEFINE_int32(mysql_port, 3306, "MySQL 端口");
DEFINE_string(mysql_user, "root", "MySQL 用户名");
DEFINE_string(mysql_password, "", "MySQL 密码；为空时读 IM_MYSQL_PASSWORD");
DEFINE_string(mysql_db, "im", "MySQL 数据库名");
DEFINE_string(mysql_charset, "utf8mb4", "MySQL 字符集");
DEFINE_int32(mysql_pool, 4, "连接池大小");

DEFINE_string(redis_host, "127.0.0.1", "Redis 地址");
DEFINE_int32(redis_port, 6379, "Redis 端口");
DEFINE_int32(redis_db, 0, "Redis 库号");
DEFINE_bool(redis_keep_alive, true, "Redis 长连接");
DEFINE_int32(session_ttl_days, 7, "登录会话有效期（天）");

namespace im {

namespace {

/// 头像大小上限 2MB
constexpr size_t kMaxAvatarSize = 2 * 1024 * 1024;

/// 个人签名长度上限
constexpr size_t kMaxDescriptionSize = 128;

/// 批量查询上限
constexpr int kMaxBatchUsers = 100;

/**
 * @brief 从环境变量读取 MySQL 密码（避免把密码写进配置/命令）。
 */
std::string mysqlPassword() {
    if (!FLAGS_mysql_password.empty()) {
        return FLAGS_mysql_password;
    }
    const char* env = std::getenv("IM_MYSQL_PASSWORD");
    return env == nullptr ? std::string() : std::string(env);
}

} // namespace

/**
 * @brief 实现构造：初始化 DAO。
 */
UserServiceImpl::UserServiceImpl(std::shared_ptr<odb::core::database> db,
                                 std::shared_ptr<SessionStore> session,
                                 std::shared_ptr<OnlineStore> online,
                                 ServiceManager::Ptr channels,
                                 std::shared_ptr<ESClient> es,
                                 std::string file_service)
    : db_(std::move(db)),
      users_(std::make_shared<UserDao>(db_)),
      session_(std::move(session)),
      online_(std::move(online)),
      channels_(std::move(channels)),
      es_(std::move(es)),
      file_service_(std::move(file_service)) {}

/**
 * @brief 实现注册：校验格式 -> account 查重 -> 加盐哈希 -> 入库。
 */
void UserServiceImpl::UserRegister(google::protobuf::RpcController* /*controller*/,
                                   const UserRegisterReq* request,
                                   UserRegisterRsp* response,
                                   google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    auto fail = [response](const std::string& msg) {
        response->set_success(false);
        response->set_errmsg(msg);
    };

    if (!validAccount(request->account())) {
        return fail("account 格式不合法");
    }
    if (!validNickname(request->nickname())) {
        return fail("昵称格式不合法");
    }
    if (!validPassword(request->password())) {
        return fail("密码格式不合法");
    }
    if (users_->findByAccount(request->account()) != nullptr) {
        return fail("account 已被占用");
    }

    const std::string salt = randomHex(16);
    const std::string hash = sha256Hex(salt + request->password());
    User user(uuid(), request->account(), request->nickname(), hash, salt);
    if (!users_->insert(user)) {
        return fail("注册失败");
    }

    es_->upsertUser(user.user_id(), user.account(), user.nickname());

    response->set_success(true);
    response->set_user_id(user.user_id());
    IM_INFO("register account={} user_id={}", request->account(), user.user_id());
}

/**
 * @brief 实现登录：校验密码 -> 单点登录检查 -> 写会话与在线标记。
 */
void UserServiceImpl::UserLogin(google::protobuf::RpcController* /*controller*/,
                                const UserLoginReq* request,
                                UserLoginRsp* response,
                                google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    auto fail = [response](const std::string& msg) {
        response->set_success(false);
        response->set_errmsg(msg);
    };

    auto user = users_->findByAccount(request->account());
    if (user == nullptr || sha256Hex(user->salt() + request->password()) !=
                                user->password_hash()) {
        return fail("用户名或密码错误");
    }
    if (online_->exists(user->user_id())) {
        return fail("用户已在其他设备登录");
    }

    const std::string ssid = uuid();
    session_->save(ssid, user->user_id());
    online_->save(user->user_id(), ssid);

    response->set_success(true);
    response->set_session_id(ssid);
    IM_INFO("login account={} user_id={}", request->account(), user->user_id());
}

/**
 * @brief 实现登出：删除会话与在线标记。
 */
void UserServiceImpl::UserLogout(google::protobuf::RpcController* /*controller*/,
                                 const UserLogoutReq* request,
                                 UserLogoutRsp* response,
                                 google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    if (!request->session_id().empty()) {
        session_->remove(request->session_id());
    }
    if (!request->user_id().empty()) {
        online_->remove(request->user_id());
    }

    response->set_success(true);
    IM_INFO("logout user_id={}", request->user_id());
}

/**
 * @brief 实现获取用户信息；头像按需从 file 服务拉取。
 */
void UserServiceImpl::GetUserInfo(google::protobuf::RpcController* /*controller*/,
                                  const GetUserInfoReq* request,
                                  GetUserInfoRsp* response,
                                  google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    if (request->user_id().empty()) {
        response->set_success(false);
        response->set_errmsg("未登录或会话已过期");
        return;
    }

    auto user = users_->findByUserId(request->user_id());
    if (user == nullptr) {
        response->set_success(false);
        response->set_errmsg("用户不存在");
        return;
    }

    UserInfo* info = response->mutable_user_info();
    info->set_user_id(user->user_id());
    info->set_nickname(user->nickname());
    info->set_description(user->description());

    if (!user->avatar_id().empty()) {
        std::string avatar;
        if (fetchAvatar(request->request_id(), user->avatar_id(), avatar)) {
            info->set_avatar(std::move(avatar));
        }
    }

    response->set_success(true);
}

/**
 * @brief 实现批量获取用户信息，单次最多 100 个。
 */
void UserServiceImpl::GetMultiUserInfo(google::protobuf::RpcController* /*controller*/,
                                       const GetMultiUserInfoReq* request,
                                       GetMultiUserInfoRsp* response,
                                       google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    if (request->user_ids_size() > kMaxBatchUsers) {
        response->set_success(false);
        response->set_errmsg("一次最多查询 100 个用户");
        return;
    }

    for (const auto& user_id : request->user_ids()) {
        auto user = users_->findByUserId(user_id);
        if (user == nullptr) {
            continue;
        }
        UserInfo info;
        info.set_user_id(user->user_id());
        info.set_nickname(user->nickname());
        info.set_description(user->description());
        (*response->mutable_users())[user_id] = std::move(info);
    }

    response->set_success(true);
}

/**
 * @brief 实现修改昵称（仅展示名，不要求唯一）。
 */
void UserServiceImpl::SetUserNickname(google::protobuf::RpcController* /*controller*/,
                                      const SetUserNicknameReq* request,
                                      SetUserNicknameRsp* response,
                                      google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    auto fail = [response](const std::string& msg) {
        response->set_success(false);
        response->set_errmsg(msg);
    };

    if (request->user_id().empty()) {
        return fail("未登录或会话已过期");
    }
    if (!validNickname(request->nickname())) {
        return fail("昵称格式不合法");
    }

    auto user = users_->findByUserId(request->user_id());
    if (user == nullptr) {
        return fail("用户不存在");
    }
    user->nickname(request->nickname());
    if (!users_->update(*user)) {
        return fail("修改昵称失败");
    }
    es_->upsertUser(user->user_id(), user->account(), user->nickname());
    response->set_success(true);
}

/**
 * @brief 实现修改个人签名。
 */
void UserServiceImpl::SetUserDescription(google::protobuf::RpcController* /*controller*/,
                                         const SetUserDescriptionReq* request,
                                         SetUserDescriptionRsp* response,
                                         google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    auto fail = [response](const std::string& msg) {
        response->set_success(false);
        response->set_errmsg(msg);
    };

    if (request->user_id().empty()) {
        return fail("未登录或会话已过期");
    }
    if (request->description().size() > kMaxDescriptionSize) {
        return fail("签名长度超过限制");
    }

    auto user = users_->findByUserId(request->user_id());
    if (user == nullptr) {
        return fail("用户不存在");
    }
    user->description(request->description());
    if (!users_->update(*user)) {
        return fail("修改签名失败");
    }
    response->set_success(true);
}

/**
 * @brief 实现修改头像：校验大小 -> 上传 file 服务 -> 更新 avatar_id。
 */
void UserServiceImpl::SetUserAvatar(google::protobuf::RpcController* /*controller*/,
                                    const SetUserAvatarReq* request,
                                    SetUserAvatarRsp* response,
                                    google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    auto fail = [response](const std::string& msg) {
        response->set_success(false);
        response->set_errmsg(msg);
    };

    if (request->user_id().empty()) {
        return fail("未登录或会话已过期");
    }
    if (request->avatar().size() > kMaxAvatarSize) {
        return fail("头像大小超过限制");
    }

    auto user = users_->findByUserId(request->user_id());
    if (user == nullptr) {
        return fail("用户不存在");
    }

    std::string file_id;
    if (!uploadAvatar(request->request_id(), request->avatar(), file_id)) {
        return fail("头像上传失败");
    }

    user->avatar_id(file_id);
    if (!users_->update(*user)) {
        return fail("修改头像失败");
    }
    response->set_success(true);
}

/**
 * @brief 实现头像下载：通过服务发现选择 file 节点并调用 GetFile。
 */
bool UserServiceImpl::fetchAvatar(const std::string& request_id,
                                  const std::string& avatar_id,
                                  std::string& content) {
    auto channel = channels_->choose(file_service_);
    if (!channel) {
        IM_WARN("{} no file service node, skip avatar", request_id);
        return false;
    }

    FileService_Stub stub(channel.get());
    GetFileRequest request;
    request.set_request_id(request_id);
    request.set_file_id(avatar_id);
    GetFileResponse response;
    brpc::Controller cntl;
    stub.GetFile(&cntl, &request, &response, nullptr);

    if (cntl.Failed() || !response.success()) {
        IM_WARN("{} fetch avatar {} failed", request_id, avatar_id);
        return false;
    }
    content = std::move(*response.mutable_content());
    return true;
}

/**
 * @brief 实现头像上传：调用 file 服务的 PutFile。
 */
bool UserServiceImpl::uploadAvatar(const std::string& request_id,
                                   const std::string& content,
                                   std::string& file_id) {
    auto channel = channels_->choose(file_service_);
    if (!channel) {
        IM_ERROR("{} no file service node", request_id);
        return false;
    }

    FileService_Stub stub(channel.get());
    PutFileRequest request;
    request.set_request_id(request_id);
    request.set_file_name("avatar");
    request.set_content(content);
    PutFileResponse response;
    brpc::Controller cntl;
    stub.PutFile(&cntl, &request, &response, nullptr);

    if (cntl.Failed() || !response.success()) {
        IM_ERROR("{} upload avatar failed", request_id);
        return false;
    }
    file_id = response.file_id();
    return true;
}

/**
 * @brief account 校验：4–20 位，字母开头，仅字母/数字/下划线。
 */
bool UserServiceImpl::validAccount(const std::string& account) {
    if (account.size() < 4 || account.size() > 20) {
        return false;
    }
    if (!std::isalpha(static_cast<unsigned char>(account[0]))) {
        return false;
    }
    for (char c : account) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            return false;
        }
    }
    return true;
}

/**
 * @brief nickname 校验：3–16 位，字母/数字/下划线/连字符。
 */
bool UserServiceImpl::validNickname(const std::string& nickname) {
    if (nickname.size() < 3 || nickname.size() > 16) {
        return false;
    }
    for (char c : nickname) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

/**
 * @brief password 校验：6–32 位可见 ASCII。
 */
bool UserServiceImpl::validPassword(const std::string& password) {
    if (password.size() < 6 || password.size() > 32) {
        return false;
    }
    for (char c : password) {
        if (c < 0x21 || c > 0x7e) {
            return false;
        }
    }
    return true;
}

} // namespace im

/**
 * @brief 程序入口：初始化依赖 -> 起 RPC 服务 -> 注册 etcd -> 阻塞运行。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    im::init_logger(FLAGS_run_mode, FLAGS_log_dir, FLAGS_log_level);

    auto db = im::ODBFactory::create(FLAGS_mysql_user, im::mysqlPassword(),
                                     FLAGS_mysql_host, FLAGS_mysql_db,
                                     FLAGS_mysql_charset,
                                     static_cast<unsigned int>(FLAGS_mysql_port),
                                     FLAGS_mysql_pool);
    auto redis = im::RedisClientFactory::create(FLAGS_redis_host, FLAGS_redis_port,
                                                FLAGS_redis_db, FLAGS_redis_keep_alive);
    auto ttl = std::chrono::seconds(FLAGS_session_ttl_days * 24 * 3600);
    auto session = std::make_shared<im::SessionStore>(redis, ttl);
    auto online = std::make_shared<im::OnlineStore>(redis, ttl);

    auto es = std::make_shared<im::ESClient>(FLAGS_es_host, FLAGS_es_index);
    es->ensureIndex();

    auto channels = std::make_shared<im::ServiceManager>(FLAGS_service_base);
    channels->follow(FLAGS_file_service);
    auto discovery = std::make_shared<im::Discovery>(
        FLAGS_etcd_host, FLAGS_service_base,
        [channels](const std::string& key, const std::string& host) {
            channels->onServiceOnline(key, host);
        },
        [channels](const std::string& key, const std::string& host) {
            channels->onServiceOffline(key, host);
        });

    brpc::Server server;
    im::UserServiceImpl service(db, session, online, channels, es, FLAGS_file_service);
    if (server.AddService(&service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        IM_FATAL("add user service failed");
        return -1;
    }

    brpc::ServerOptions options;
    options.num_threads = FLAGS_rpc_threads;
    if (server.Start(FLAGS_listen_port, &options) != 0) {
        IM_FATAL("start user server on port {} failed", FLAGS_listen_port);
        return -1;
    }

    const std::string instance =
        FLAGS_instance_id.empty() ? FLAGS_advertise_host : FLAGS_instance_id;
    im::Registry registry(FLAGS_etcd_host, FLAGS_service_base);
    if (!registry.registerService(FLAGS_service_name, instance, FLAGS_advertise_host)) {
        IM_FATAL("register service {} failed", FLAGS_service_name);
        return -1;
    }

    IM_INFO("user_server listening on 0.0.0.0:{}", FLAGS_listen_port);
    server.RunUntilAskedToQuit();
    return 0;
}
