/**
 * @file ws_client.cpp
 * @brief WebSocket 验收客户端：鉴权 -> HTTP 发消息 -> 等待推送。
 */

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

#include <gflags/gflags.h>
#include <httplib.h>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "base.pb.h"
#include "transmit.pb.h"

DEFINE_string(host, "127.0.0.1", "网关地址");
DEFINE_int32(ws_port, 9001, "WebSocket 端口");
DEFINE_int32(http_port, 9000, "HTTP 端口");
DEFINE_string(session_id, "", "登录会话 ID");
DEFINE_string(chat_session_id, "", "目标会话 ID");
DEFINE_string(text, "ws-test", "发送的文本");

using WsClient = websocketpp::client<websocketpp::config::asio_client>;

namespace {
std::mutex g_mutex;
std::condition_variable g_cv;
bool g_got = false;
} // namespace

/**
 * @brief 程序入口。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);

    WsClient client;
    client.init_asio();

    client.set_open_handler([&client](websocketpp::connection_hdl hdl) {
        im::ClientAuthenticationReq auth;
        auth.set_session_id(FLAGS_session_id);
        client.get_con_from_hdl(hdl)->send(auth.SerializeAsString(),
                                           websocketpp::frame::opcode::binary);

        std::thread([&client]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            httplib::Client http(FLAGS_host, FLAGS_http_port);
            im::SendMessageReq request;
            request.set_session_id(FLAGS_session_id);
            request.set_chat_session_id(FLAGS_chat_session_id);
            request.mutable_message()->set_message_type(im::MessageType::STRING);
            request.mutable_message()->mutable_string_message()->set_content(FLAGS_text);
            auto response = http.Post("/api/message/send", request.SerializeAsString(),
                                      "application/x-protobuf");
            std::printf("http send status=%d\n", response ? response->status : -1);
        }).detach();
    });

    client.set_message_handler([](websocketpp::connection_hdl,
                                  WsClient::message_ptr msg) {
        im::NotifyMessage notify;
        if (!notify.ParseFromString(msg->get_payload())) {
            std::printf("bad notify frame\n");
            return;
        }
        std::printf("notify type=%d message_id=%s\n", notify.type(),
                    notify.message().message_id().c_str());
        std::lock_guard<std::mutex> lock(g_mutex);
        g_got = true;
        g_cv.notify_one();
    });

    client.set_fail_handler([](websocketpp::connection_hdl) {
        std::printf("ws connect failed\n");
    });

    websocketpp::lib::error_code ec;
    auto conn = client.get_connection(
        "ws://" + FLAGS_host + ":" + std::to_string(FLAGS_ws_port), ec);
    if (ec) {
        std::printf("create connection failed: %s\n", ec.message().c_str());
        return 1;
    }
    client.connect(conn);

    std::thread runner([&client]() { client.run(); });

    bool received = false;
    {
        std::unique_lock<std::mutex> lock(g_mutex);
        received = g_cv.wait_for(lock, std::chrono::seconds(6), [] { return g_got; });
    }

    client.close(conn, websocketpp::close::status::normal, "done");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    client.stop();
    if (runner.joinable()) {
        runner.join();
    }

    std::printf("%s\n", received ? "ALL PASS" : "NO NOTIFY");
    return received ? 0 : 1;
}
