/**
 * @file echo_client.cpp
 * @brief M0 验收客户端：连接 echo_server，发一条消息并打印回显。
 *
 * 用途：配合 services/echo 验证 RPC 链路；退出码 0 表示调用成功，非 0 表示失败，
 * 方便脚本化验收（./bin/echo_client && echo PASS）。
 */

#include <cstdio>

#include <brpc/channel.h>
#include <gflags/gflags.h>

#include "logger.hpp"
#include "echo.pb.h"

// 连接参数：默认连本机 10000 端口，可通过命令行或 flagfile 覆盖
DEFINE_string(host, "127.0.0.1:10000", "echo 服务地址 host:port");
DEFINE_string(msg, "hello", "要发送的消息");
DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_file, "logs/echo_client.log", "发布模式下的日志文件");
DEFINE_int32(log_level, 0, "日志等级");

/**
 * @brief 程序入口：解析参数 → 建连 → 发起 Echo 调用 → 打印结果。
 * @return 0 成功；1 建连失败或 RPC 失败
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    im::init_logger(FLAGS_run_mode, FLAGS_log_file, FLAGS_log_level);

    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.timeout_ms = 3000;
    options.max_retry = 1;
    options.protocol = "baidu_std";

    if (channel.Init(FLAGS_host.c_str(), &options) != 0) {
        IM_ERROR("init channel to {} failed", FLAGS_host);
        return 1;
    }

    im::EchoService_Stub stub(&channel);
    brpc::Controller cntl;
    im::EchoRequest request;
    im::EchoResponse response;
    request.set_message(FLAGS_msg);

    stub.Echo(&cntl, &request, &response, nullptr);
    if (cntl.Failed()) {
        IM_ERROR("rpc failed: {}", cntl.ErrorText());
        return 1;
    }

    std::printf("%s\n", response.message().c_str());
    IM_INFO("rpc ok, server_time_ms={}", response.server_time_ms());
    return 0;
}
