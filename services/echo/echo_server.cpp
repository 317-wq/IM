/**
 * @file echo_server.cpp
 * @brief M0 示例服务：验证 protoc 生成代码、brpc 服务端与统一日志是否打通。
 *
 * 这里只有一个 Echo 接口，不承载业务；目的是先把「构建 → 生成 → 起服务 → 客户端调用」
 * 这条最小闭环跑通，后续里程碑的新服务都复用同一套骨架。
 */

#include <brpc/server.h>
#include <gflags/gflags.h>

#include "common/logger.hpp"
#include "common/utils.hpp"
#include "echo.pb.h"

// 运行参数：既可直接命令行传，也可用 -flagfile=conf/echo.conf 批量指定
DEFINE_bool(run_mode, false, "运行模式：false-调试（控制台），true-发布（文件）");
DEFINE_string(log_file, "logs/echo.log", "发布模式下的日志文件");
DEFINE_int32(log_level, 0, "日志等级：0 trace / 1 debug / 2 info / 3 warn / 4 error");
DEFINE_int32(port, 10000, "RPC 监听端口");

namespace im {

/**
 * @brief Echo 服务的实现类。
 *
 * 只做一件事：把客户端发来的文本加上 "echo: " 前缀回显，
 * 并带上服务端当前毫秒时间戳，方便调用方确认请求确实到达了服务端。
 */
class EchoServiceImpl : public EchoService {
public:
    /**
     * @brief 处理一次 Echo 调用。
     * @param request  客户端请求（message 为待回显文本）
     * @param response 响应（message 为回显内容，server_time_ms 为服务端时间）
     * @param done     brpc 的完成回调；ClosureGuard 保证离开作用域时一定被调用
     */
    void Echo(google::protobuf::RpcController* /*controller*/,
              const EchoRequest* request,
              EchoResponse* response,
              google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        response->set_message("echo: " + request->message());
        response->set_server_time_ms(now_ms());
        IM_DEBUG("handle echo, msg={}", request->message());
    }
};

} // namespace im

/**
 * @brief 程序入口：解析参数 → 初始化日志 → 注册服务 → 启动并阻塞。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    im::init_logger(FLAGS_run_mode, FLAGS_log_file, FLAGS_log_level);

    brpc::Server server;
    im::EchoServiceImpl service;

    if (server.AddService(&service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        IM_FATAL("add echo service failed");
        return -1;
    }

    brpc::ServerOptions options;
    options.num_threads = 4;
    if (server.Start(FLAGS_port, &options) != 0) {
        IM_FATAL("start server on port {} failed", FLAGS_port);
        return -1;
    }

    IM_INFO("echo_server listening on 0.0.0.0:{}", FLAGS_port);
    server.RunUntilAskedToQuit();
    return 0;
}
