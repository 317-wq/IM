/**
 * @file message_client.cpp
 * @brief M6 消息查询验收客户端：history / recent / search。
 */

#include <cstdio>
#include <string>

#include <brpc/channel.h>
#include <gflags/gflags.h>

#include "channel.hpp"
#include "etcd.hpp"
#include "logger.hpp"
#include "message.pb.h"
#include "utils.hpp"

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd 地址");
DEFINE_string(service_base, "/im/service", "服务注册根目录");
DEFINE_string(service_name, "message", "目标服务名");
DEFINE_string(op, "history", "操作：history/recent/search");
DEFINE_string(chat_session_id, "", "会话 ID");
DEFINE_string(user_id, "", "当前用户 user_id");
DEFINE_int64(start_time, 0, "历史起始时间（秒），0 表示从最早开始");
DEFINE_int64(end_time, 0, "历史结束时间（秒），0 表示当前时间");
DEFINE_int64(count, 20, "最近消息条数");
DEFINE_string(key, "", "搜索关键字");
DEFINE_int32(rpc_timeout_ms, 5000, "RPC 超时（毫秒）");
DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/message_client", "发布模式日志目录");
DEFINE_int32(log_level, 0, "日志等级");

namespace {

/**
 * @brief 打印一条消息。
 */
void printMessage(const im::MessageInfo& message) {
    const auto& content = message.message();
    std::string body;
    if (content.message_type() == im::MessageType::STRING) {
        body = content.string_message().content();
    } else if (content.message_type() == im::MessageType::IMAGE) {
        body = "image:" + content.image_message().file_id();
    } else if (content.message_type() == im::MessageType::FILE) {
        body = "file:" + content.file_message().file_id() + ":" +
               content.file_message().file_name();
    } else if (content.message_type() == im::MessageType::SPEECH) {
        body = "speech:" + content.speech_message().file_id();
    }
    std::printf("id=%s sender=%s type=%d content=%s\n", message.message_id().c_str(),
                message.sender().nickname().c_str(),
                static_cast<int>(content.message_type()), body.c_str());
}

} // namespace

/**
 * @brief 程序入口：发现服务 -> 按 op 查询。
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
        std::printf("%s failed: 没有可用的 message 服务节点\n", FLAGS_op.c_str());
        return 1;
    }

    im::MessageService_Stub stub(channel.get());
    brpc::Controller cntl;
    cntl.set_timeout_ms(FLAGS_rpc_timeout_ms);
    const std::string request_id = im::uuid();

    if (FLAGS_op == "history") {
        im::GetHistoryMsgReq request;
        request.set_request_id(request_id);
        request.set_chat_session_id(FLAGS_chat_session_id);
        request.set_start_time(FLAGS_start_time);
        request.set_end_time(FLAGS_end_time == 0 ? im::now_s() : FLAGS_end_time);
        im::GetHistoryMsgRsp response;
        stub.GetHistory(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            std::printf("history failed: %s\n",
                        cntl.Failed() ? cntl.ErrorText().c_str() : response.errmsg().c_str());
            return 1;
        }
        std::printf("count=%d\n", response.messages_size());
        for (const auto& message : response.messages()) {
            printMessage(message);
        }
        return 0;
    }

    if (FLAGS_op == "recent") {
        im::GetRecentMsgReq request;
        request.set_request_id(request_id);
        request.set_chat_session_id(FLAGS_chat_session_id);
        request.set_count(FLAGS_count);
        im::GetRecentMsgRsp response;
        stub.GetRecent(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            std::printf("recent failed: %s\n",
                        cntl.Failed() ? cntl.ErrorText().c_str() : response.errmsg().c_str());
            return 1;
        }
        std::printf("count=%d\n", response.messages_size());
        for (const auto& message : response.messages()) {
            printMessage(message);
        }
        return 0;
    }

    if (FLAGS_op == "search") {
        im::MsgSearchReq request;
        request.set_request_id(request_id);
        request.set_chat_session_id(FLAGS_chat_session_id);
        request.set_search_key(FLAGS_key);
        im::MsgSearchRsp response;
        stub.Search(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            std::printf("search failed: %s\n",
                        cntl.Failed() ? cntl.ErrorText().c_str() : response.errmsg().c_str());
            return 1;
        }
        std::printf("count=%d\n", response.messages_size());
        for (const auto& message : response.messages()) {
            printMessage(message);
        }
        return 0;
    }

    std::printf("%s failed: 未知操作\n", FLAGS_op.c_str());
    return 1;
}
