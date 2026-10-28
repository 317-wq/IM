/**
 * @file echo_server.cpp
 * @brief M0 回显服务实现与程序入口。
 *
 * 类声明在 echo_server.hpp，这里只放接口实现和 main()；
 * 目的是先把「构建 → protoc 生成 → 起服务 → 客户端调用」这条最小闭环跑通，
 * 后续里程碑的新服务都复用同一套骨架。
 */

#include "echo_server.hpp"

#include <brpc/server.h>
#include <gflags/gflags.h>

#include "logger.hpp"
#include "utils.hpp"

// 运行参数：既可直接命令行传，也可用 -flagfile=conf/echo.conf 批量指定
DEFINE_bool(run_mode, false, "运行模式：false-调试（控制台），true-发布（文件）");
DEFINE_string(log_dir, "logs/echo", "发布模式下的日志目录（按天生成 YYYY-MM-DD.log）");
DEFINE_int32(log_level, 0, "日志等级：0 trace / 1 debug / 2 info / 3 warn / 4 error");
DEFINE_int32(port, 10000, "RPC 监听端口");

namespace im {

/**
 * @brief 实现 Echo：回显文本 + 返回服务端毫秒时间戳。
 */
void EchoServiceImpl::Echo(google::protobuf::RpcController* /*controller*/,
                           const EchoRequest* request,
                           EchoResponse* response,
                           google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_message("echo: " + request->message());
    response->set_server_time_ms(now_ms());
    IM_DEBUG("handle echo, msg={}", request->message());
}

} // namespace im

/**
 * @brief 程序入口：解析参数 → 初始化日志 → 注册服务 → 启动并阻塞。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    im::init_logger(FLAGS_run_mode, FLAGS_log_dir, FLAGS_log_level);

    brpc::Server server;
    im::EchoServiceImpl service;

    if (server.AddService(&service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        IM_FATAL("add echo service failed");
        return -1;
    }

    brpc::ServerOptions options;
    options.num_threads = 9;
    if (server.Start(FLAGS_port, &options) != 0) {
        IM_FATAL("start server on port {} failed", FLAGS_port);
        return -1;
    }

    IM_INFO("echo_server listening on 0.0.0.0:{}", FLAGS_port);
    server.RunUntilAskedToQuit();
    return 0;
}
