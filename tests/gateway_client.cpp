/**
 * @file gateway_client.cpp
 * @brief M7 网关验收客户端（HTTP + protobuf）。
 */

#include <cstdio>
#include <string>

#include <gflags/gflags.h>
#include <httplib.h>

#include "logger.hpp"
#include "friend.pb.h"
#include "message.pb.h"
#include "transmit.pb.h"
#include "user.pb.h"
#include "utils.hpp"

DEFINE_string(host, "127.0.0.1", "网关地址");
DEFINE_int32(port, 9000, "网关端口");
DEFINE_string(op, "register", "操作：register/login/info/history");
DEFINE_string(account, "", "登录名");
DEFINE_string(nickname, "", "昵称");
DEFINE_string(password, "", "密码");
DEFINE_string(session_id, "", "登录会话 ID");
DEFINE_string(chat_session_id, "", "会话 ID");
DEFINE_string(text, "hello-gateway", "发送的文本内容");
DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/gateway_client", "发布模式日志目录");
DEFINE_int32(log_level, 0, "日志等级");

namespace {

/**
 * @brief 发送 protobuf 请求并解析响应。
 */
template <typename ReqT, typename RspT>
bool post(httplib::Client& client, const std::string& path, ReqT& request, RspT& response) {
    request.set_request_id(im::uuid());
    auto result = client.Post(path.c_str(), request.SerializeAsString(),
                              "application/x-protobuf");
    if (!result || result->status != 200) {
        return false;
    }
    return response.ParseFromString(result->body);
}

} // namespace

/**
 * @brief 程序入口。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    im::init_logger(FLAGS_run_mode, FLAGS_log_dir, FLAGS_log_level);

    httplib::Client client(FLAGS_host, FLAGS_port);
    client.set_connection_timeout(3, 0);
    client.set_read_timeout(5, 0);

    if (FLAGS_op == "register") {
        im::UserRegisterReq request;
        request.set_account(FLAGS_account);
        request.set_nickname(FLAGS_nickname);
        request.set_password(FLAGS_password);
        im::UserRegisterRsp response;
        if (!post(client, "/api/user/register", request, response) || !response.success()) {
            std::printf("register failed: %s\n", response.errmsg().c_str());
            return 1;
        }
        std::printf("user_id=%s\n", response.user_id().c_str());
        return 0;
    }

    if (FLAGS_op == "login") {
        im::UserLoginReq request;
        request.set_account(FLAGS_account);
        request.set_password(FLAGS_password);
        im::UserLoginRsp response;
        if (!post(client, "/api/user/login", request, response) || !response.success()) {
            std::printf("login failed: %s\n", response.errmsg().c_str());
            return 1;
        }
        std::printf("session_id=%s\n", response.session_id().c_str());
        return 0;
    }

    if (FLAGS_op == "info") {
        im::GetUserInfoReq request;
        request.set_session_id(FLAGS_session_id);
        im::GetUserInfoRsp response;
        if (!post(client, "/api/user/info", request, response) || !response.success()) {
            std::printf("info failed: %s\n", response.errmsg().c_str());
            return 1;
        }
        std::printf("user_id=%s nickname=%s\n", response.user_info().user_id().c_str(),
                    response.user_info().nickname().c_str());
        return 0;
    }

    if (FLAGS_op == "history") {
        im::GetHistoryMsgReq request;
        request.set_session_id(FLAGS_session_id);
        request.set_chat_session_id(FLAGS_chat_session_id);
        request.set_end_time(im::now_s());
        im::GetHistoryMsgRsp response;
        if (!post(client, "/api/message/history", request, response) ||
            !response.success()) {
            std::printf("history failed: %s\n", response.errmsg().c_str());
            return 1;
        }
        std::printf("count=%d\n", response.messages_size());
        for (const auto& message : response.messages()) {
            std::printf("id=%s type=%d\n", message.message_id().c_str(),
                        static_cast<int>(message.message().message_type()));
        }
        return 0;
    }

    if (FLAGS_op == "friend_list") {
        im::GetFriendListReq request;
        request.set_session_id(FLAGS_session_id);
        im::GetFriendListRsp response;
        if (!post(client, "/api/friend/list", request, response) || !response.success()) {
            std::printf("friend_list failed: %s\n", response.errmsg().c_str());
            return 1;
        }
        std::printf("count=%d\n", response.friends_size());
        for (const auto& user : response.friends()) {
            std::printf("user_id=%s nickname=%s\n", user.user_id().c_str(),
                        user.nickname().c_str());
        }
        return 0;
    }

    if (FLAGS_op == "sessions") {
        im::GetChatSessionListReq request;
        request.set_session_id(FLAGS_session_id);
        im::GetChatSessionListRsp response;
        if (!post(client, "/api/session/list", request, response) || !response.success()) {
            std::printf("sessions failed: %s\n", response.errmsg().c_str());
            return 1;
        }
        std::printf("count=%d\n", response.sessions_size());
        for (const auto& session : response.sessions()) {
            std::printf("session_id=%s type=%d\n", session.session_id().c_str(),
                        session.type());
        }
        return 0;
    }

    if (FLAGS_op == "send") {
        im::SendMessageReq request;
        request.set_session_id(FLAGS_session_id);
        request.set_chat_session_id(FLAGS_chat_session_id);
        request.mutable_message()->set_message_type(im::MessageType::STRING);
        request.mutable_message()->mutable_string_message()->set_content(FLAGS_text);
        im::SendMessageRsp response;
        if (!post(client, "/api/message/send", request, response) || !response.success()) {
            std::printf("send failed: %s\n", response.errmsg().c_str());
            return 1;
        }
        std::printf("message_id=%s targets=%d\n", response.message().message_id().c_str(),
                    response.target_ids_size());
        return 0;
    }

    if (FLAGS_op == "recent") {
        im::GetRecentMsgReq request;
        request.set_session_id(FLAGS_session_id);
        request.set_chat_session_id(FLAGS_chat_session_id);
        request.set_count(5);
        im::GetRecentMsgRsp response;
        if (!post(client, "/api/message/recent", request, response) || !response.success()) {
            std::printf("recent failed: %s\n", response.errmsg().c_str());
            return 1;
        }
        std::printf("count=%d\n", response.messages_size());
        return 0;
    }

    std::printf("%s failed: 未知操作\n", FLAGS_op.c_str());
    return 1;
}
