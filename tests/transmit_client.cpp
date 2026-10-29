/**
 * @file transmit_client.cpp
 * @brief M5 消息转发服务验收客户端。
 */

#include <cstdio>
#include <string>

#include <brpc/channel.h>
#include <gflags/gflags.h>

#include "channel.hpp"
#include "etcd.hpp"
#include "logger.hpp"
#include "transmit.pb.h"
#include "utils.hpp"

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd 地址");
DEFINE_string(service_base, "/im/service", "服务注册根目录");
DEFINE_string(service_name, "transmit", "目标服务名");
DEFINE_string(user_id, "", "发送者 user_id");
DEFINE_string(session_id, "", "登录会话 ID");
DEFINE_string(chat_session_id, "", "目标会话 ID");
DEFINE_string(text, "hello", "文本消息内容");
DEFINE_string(op, "text", "消息类型：text/image");
DEFINE_string(image, "", "图片文件路径（op=image 时使用）");
DEFINE_int32(rpc_timeout_ms, 5000, "RPC 超时（毫秒）");
DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/transmit_client", "发布模式日志目录");
DEFINE_int32(log_level, 0, "日志等级");

/**
 * @brief 程序入口。
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
        std::printf("send failed: 没有可用的 transmit 服务节点\n");
        return 1;
    }

    im::TransmitService_Stub stub(channel.get());
    brpc::Controller cntl;
    cntl.set_timeout_ms(FLAGS_rpc_timeout_ms);

    im::SendMessageReq request;
    request.set_request_id(im::uuid());
    request.set_user_id(FLAGS_user_id);
    request.set_session_id(FLAGS_session_id);
    request.set_chat_session_id(FLAGS_chat_session_id);

    if (FLAGS_op == "image") {
        std::string image;
        if (!im::readFile(FLAGS_image, image)) {
            std::printf("send failed: 读取图片失败\n");
            return 1;
        }
        request.mutable_message()->set_message_type(im::MessageType::IMAGE);
        request.mutable_message()->mutable_image_message()->set_image_content(
            std::move(image));
    } else {
        request.mutable_message()->set_message_type(im::MessageType::STRING);
        request.mutable_message()->mutable_string_message()->set_content(FLAGS_text);
    }

    im::SendMessageRsp response;
    stub.SendMessage(&cntl, &request, &response, nullptr);
    if (cntl.Failed() || !response.success()) {
        std::printf("send failed: %s\n",
                    cntl.Failed() ? cntl.ErrorText().c_str() : response.errmsg().c_str());
        return 1;
    }

    std::printf("message_id=%s targets=%d\n", response.message().message_id().c_str(),
                response.target_ids_size());
    return 0;
}
