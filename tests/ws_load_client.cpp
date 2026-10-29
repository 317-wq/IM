/**
 * @file ws_load_client.cpp
 * @brief WebSocket 连接容量测试：登录 N 个账号 -> 建立 N 条 WS 连接并鉴权。
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <gflags/gflags.h>
#include <httplib.h>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "base.pb.h"
#include "user.pb.h"
#include "utils.hpp"

DEFINE_string(host, "127.0.0.1", "网关地址");
DEFINE_int32(http_port, 9000, "HTTP 端口");
DEFINE_int32(ws_port, 9001, "WebSocket 端口");
DEFINE_int32(count, 100, "连接数");
DEFINE_string(prefix, "cap", "账号前缀");
DEFINE_string(password, "123456", "账号密码");
DEFINE_int32(hold_seconds, 10, "连接保持秒数");

using WsClient = websocketpp::client<websocketpp::config::asio_client>;

namespace {
std::atomic<int> g_opened{0};
std::atomic<int> g_failed{0};
} // namespace

/**
 * @brief 程序入口。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);

    std::vector<std::string> sessions;
    sessions.reserve(FLAGS_count);
    {
        httplib::Client http(FLAGS_host, FLAGS_http_port);
        http.set_connection_timeout(3, 0);
        http.set_read_timeout(10, 0);
        for (int i = 0; i < FLAGS_count; ++i) {
            char account[32];
            std::snprintf(account, sizeof(account), "%s%04d", FLAGS_prefix.c_str(), i);
            im::UserLoginReq request;
            request.set_request_id(im::uuid());
            request.set_account(account);
            request.set_password(FLAGS_password);
            im::UserLoginRsp response;
            auto result = http.Post("/api/user/login", request.SerializeAsString(),
                                    "application/x-protobuf");
            if (result && result->status == 200 &&
                response.ParseFromString(result->body) && response.success()) {
                sessions.push_back(response.session_id());
            }
        }
    }
    std::printf("logged_in=%zu/%d\n", sessions.size(), FLAGS_count);

    WsClient client;
    client.init_asio();
    client.set_access_channels(websocketpp::log::alevel::none);

    const std::string uri = "ws://" + FLAGS_host + ":" + std::to_string(FLAGS_ws_port);
    for (const auto& sid : sessions) {
        websocketpp::lib::error_code ec;
        auto conn = client.get_connection(uri, ec);
        if (ec) {
            g_failed++;
            continue;
        }
        conn->set_open_handler([&client, sid](websocketpp::connection_hdl hdl) {
            im::ClientAuthenticationReq auth;
            auth.set_session_id(sid);
            client.get_con_from_hdl(hdl)->send(auth.SerializeAsString(),
                                               websocketpp::frame::opcode::binary);
            g_opened++;
        });
        conn->set_fail_handler([](websocketpp::connection_hdl) { g_failed++; });
        client.connect(conn);
    }

    std::thread runner([&client]() { client.run(); });
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::printf("opened=%d failed=%d\n", g_opened.load(), g_failed.load());

    std::this_thread::sleep_for(std::chrono::seconds(FLAGS_hold_seconds));
    client.stop();
    if (runner.joinable()) {
        runner.join();
    }
    std::printf("final opened=%d failed=%d\n", g_opened.load(), g_failed.load());
    return 0;
}
