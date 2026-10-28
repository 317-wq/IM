/**
 * @file user_client.cpp
 * @brief M3 用户服务验收客户端。
 *
 * 通过 etcd 发现 user 服务，按 op 调用对应接口：
 *   register / login / logout / get / set_nickname / set_description / set_avatar
 */

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include <brpc/channel.h>
#include <gflags/gflags.h>

#include "channel.hpp"
#include "etcd.hpp"
#include "logger.hpp"
#include "user.pb.h"
#include "utils.hpp"

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd 地址");
DEFINE_string(service_base, "/im/service", "服务注册根目录");
DEFINE_string(service_name, "user", "目标服务名");
DEFINE_string(op, "register",
              "操作：register/login/logout/get/set_nickname/set_description/set_avatar");
DEFINE_string(account, "", "登录名");
DEFINE_string(nickname, "", "展示名");
DEFINE_string(password, "", "密码");
DEFINE_string(user_id, "", "业务用户 ID（get/set 时使用）");
DEFINE_string(session_id, "", "登录会话 ID");
DEFINE_string(value, "", "set 操作的取值");
DEFINE_string(avatar, "", "头像文件路径");
DEFINE_int32(rpc_timeout_ms, 5000, "RPC 超时（毫秒）");

DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/user_client", "发布模式下的日志目录");
DEFINE_int32(log_level, 0, "日志等级");

namespace {

/**
 * @brief 打印失败信息并返回 1。
 */
int fail(const std::string& op, const std::string& errmsg) {
    std::printf("%s failed: %s\n", op.c_str(), errmsg.c_str());
    return 1;
}

} // namespace

/**
 * @brief 程序入口：发现服务 -> 按 op 分发。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    im::init_logger(FLAGS_run_mode, FLAGS_log_dir, FLAGS_log_level);

    im::ServiceManager manager(FLAGS_service_base);
    manager.follow(FLAGS_service_name);

    im::Discovery discovery(
        FLAGS_etcd_host, FLAGS_service_base,
        [&manager](const std::string& key, const std::string& host) {
            manager.onServiceOnline(key, host);
        },
        [&manager](const std::string& key, const std::string& host) {
            manager.onServiceOffline(key, host);
        });

    auto channel = manager.choose(FLAGS_service_name);
    if (!channel) {
        return fail(FLAGS_op, "没有可用的 user 服务节点");
    }

    im::UserService_Stub stub(channel.get());
    brpc::Controller cntl;
    cntl.set_timeout_ms(FLAGS_rpc_timeout_ms);
    const std::string request_id = im::uuid();

    if (FLAGS_op == "register") {
        im::UserRegisterReq request;
        request.set_request_id(request_id);
        request.set_account(FLAGS_account);
        request.set_nickname(FLAGS_nickname);
        request.set_password(FLAGS_password);
        im::UserRegisterRsp response;
        stub.UserRegister(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("user_id=%s\n", response.user_id().c_str());
        return 0;
    }

    if (FLAGS_op == "login") {
        im::UserLoginReq request;
        request.set_request_id(request_id);
        request.set_account(FLAGS_account);
        request.set_password(FLAGS_password);
        im::UserLoginRsp response;
        stub.UserLogin(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("session_id=%s\n", response.session_id().c_str());
        return 0;
    }

    if (FLAGS_op == "logout") {
        im::UserLogoutReq request;
        request.set_request_id(request_id);
        request.set_session_id(FLAGS_session_id);
        request.set_user_id(FLAGS_user_id);
        im::UserLogoutRsp response;
        stub.UserLogout(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("logout ok\n");
        return 0;
    }

    if (FLAGS_op == "get") {
        im::GetUserInfoReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        request.set_session_id(FLAGS_session_id);
        im::GetUserInfoRsp response;
        stub.GetUserInfo(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        const auto& info = response.user_info();
        std::printf("user_id=%s nickname=%s description=%s avatar_size=%zu\n",
                    info.user_id().c_str(), info.nickname().c_str(),
                    info.description().c_str(), info.avatar().size());
        return 0;
    }

    if (FLAGS_op == "get_multi") {
        im::GetMultiUserInfoReq request;
        request.set_request_id(request_id);
        std::stringstream ss(FLAGS_user_id);
        std::string user_id;
        while (std::getline(ss, user_id, ',')) {
            if (!user_id.empty()) {
                request.add_user_ids(user_id);
            }
        }
        im::GetMultiUserInfoRsp response;
        stub.GetMultiUserInfo(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("count=%d\n", response.users_size());
        for (const auto& item : response.users()) {
            std::printf("user_id=%s nickname=%s\n", item.first.c_str(),
                        item.second.nickname().c_str());
        }
        return 0;
    }

    if (FLAGS_op == "set_nickname") {
        im::SetUserNicknameReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        request.set_session_id(FLAGS_session_id);
        request.set_nickname(FLAGS_value);
        im::SetUserNicknameRsp response;
        stub.SetUserNickname(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("set_nickname ok\n");
        return 0;
    }

    if (FLAGS_op == "set_description") {
        im::SetUserDescriptionReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        request.set_session_id(FLAGS_session_id);
        request.set_description(FLAGS_value);
        im::SetUserDescriptionRsp response;
        stub.SetUserDescription(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("set_description ok\n");
        return 0;
    }

    if (FLAGS_op == "set_avatar") {
        std::string avatar;
        if (!im::readFile(FLAGS_avatar, avatar)) {
            return fail(FLAGS_op, "读取头像文件失败");
        }
        im::SetUserAvatarReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        request.set_session_id(FLAGS_session_id);
        request.set_avatar(std::move(avatar));
        im::SetUserAvatarRsp response;
        stub.SetUserAvatar(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("set_avatar ok\n");
        return 0;
    }

    return fail(FLAGS_op, "未知操作");
}
