/**
 * @file friend_client.cpp
 * @brief M4 好友与会话服务验收客户端。
 */

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include <brpc/channel.h>
#include <gflags/gflags.h>

#include "channel.hpp"
#include "etcd.hpp"
#include "friend.pb.h"
#include "logger.hpp"
#include "utils.hpp"

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd 地址");
DEFINE_string(service_base, "/im/service", "服务注册根目录");
DEFINE_string(service_name, "friend", "目标服务名");
DEFINE_string(op, "list",
              "操作：add/process/pending/list/remove/search/sessions/create_group/members");
DEFINE_string(user_id, "", "当前用户 user_id");
DEFINE_string(session_id, "", "登录会话 ID");
DEFINE_string(peer_id, "", "对方 user_id");
DEFINE_string(event_id, "", "好友申请事件 ID");
DEFINE_string(apply_user_id, "", "申请人 user_id");
DEFINE_bool(agree, true, "是否同意申请");
DEFINE_string(value, "", "搜索关键字或群名称");
DEFINE_string(member_ids, "", "群成员 user_id，逗号分隔");
DEFINE_string(chat_session_id, "", "会话 ID");
DEFINE_int32(rpc_timeout_ms, 5000, "RPC 超时（毫秒）");
DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/friend_client", "发布模式日志目录");
DEFINE_int32(log_level, 0, "日志等级");

namespace {

/**
 * @brief 打印失败并返回 1。
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
        return fail(FLAGS_op, "没有可用的 friend 服务节点");
    }

    im::FriendService_Stub stub(channel.get());
    brpc::Controller cntl;
    cntl.set_timeout_ms(FLAGS_rpc_timeout_ms);
    const std::string request_id = im::uuid();

    if (FLAGS_op == "add") {
        im::FriendAddReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        request.set_session_id(FLAGS_session_id);
        request.set_respondent_id(FLAGS_peer_id);
        im::FriendAddRsp response;
        stub.FriendAdd(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("event_id=%s\n", response.notify_event_id().c_str());
        return 0;
    }

    if (FLAGS_op == "process") {
        im::FriendAddProcessReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        request.set_session_id(FLAGS_session_id);
        request.set_notify_event_id(FLAGS_event_id);
        request.set_apply_user_id(FLAGS_apply_user_id);
        request.set_agree(FLAGS_agree);
        im::FriendAddProcessRsp response;
        stub.FriendAddProcess(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("session_id=%s\n", response.new_session_id().c_str());
        return 0;
    }

    if (FLAGS_op == "pending") {
        im::GetPendingFriendEventListReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        im::GetPendingFriendEventListRsp response;
        stub.GetPendingFriendEventList(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("count=%d\n", response.events_size());
        for (const auto& event : response.events()) {
            std::printf("event_id=%s from=%s nickname=%s\n", event.event_id().c_str(),
                        event.sender().user_id().c_str(),
                        event.sender().nickname().c_str());
        }
        return 0;
    }

    if (FLAGS_op == "list") {
        im::GetFriendListReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        im::GetFriendListRsp response;
        stub.GetFriendList(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("count=%d\n", response.friends_size());
        for (const auto& user : response.friends()) {
            std::printf("user_id=%s nickname=%s\n", user.user_id().c_str(),
                        user.nickname().c_str());
        }
        return 0;
    }

    if (FLAGS_op == "remove") {
        im::FriendRemoveReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        request.set_peer_id(FLAGS_peer_id);
        im::FriendRemoveRsp response;
        stub.FriendRemove(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("remove ok\n");
        return 0;
    }

    if (FLAGS_op == "search") {
        im::FriendSearchReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        request.set_search_key(FLAGS_value);
        im::FriendSearchRsp response;
        stub.FriendSearch(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("count=%d\n", response.users_size());
        for (const auto& user : response.users()) {
            std::printf("user_id=%s nickname=%s\n", user.user_id().c_str(),
                        user.nickname().c_str());
        }
        return 0;
    }

    if (FLAGS_op == "sessions") {
        im::GetChatSessionListReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        im::GetChatSessionListRsp response;
        stub.GetChatSessionList(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("count=%d\n", response.sessions_size());
        for (const auto& session : response.sessions()) {
            std::printf("session_id=%s type=%d name=%s\n", session.session_id().c_str(),
                        session.type(), session.name().c_str());
        }
        return 0;
    }

    if (FLAGS_op == "create_group") {
        im::ChatSessionCreateReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        request.set_chat_session_name(FLAGS_value);
        std::stringstream ss(FLAGS_member_ids);
        std::string member_id;
        while (std::getline(ss, member_id, ',')) {
            if (!member_id.empty()) {
                request.add_member_ids(member_id);
            }
        }
        im::ChatSessionCreateRsp response;
        stub.ChatSessionCreate(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("session_id=%s\n", response.chat_session_id().c_str());
        return 0;
    }

    if (FLAGS_op == "members") {
        im::GetChatSessionMemberReq request;
        request.set_request_id(request_id);
        request.set_user_id(FLAGS_user_id);
        request.set_chat_session_id(FLAGS_chat_session_id);
        im::GetChatSessionMemberRsp response;
        stub.GetChatSessionMember(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return fail(FLAGS_op, cntl.Failed() ? cntl.ErrorText() : response.errmsg());
        }
        std::printf("count=%d\n", response.members_size());
        for (const auto& user : response.members()) {
            std::printf("user_id=%s nickname=%s\n", user.user_id().c_str(),
                        user.nickname().c_str());
        }
        return 0;
    }

    return fail(FLAGS_op, "未知操作");
}
