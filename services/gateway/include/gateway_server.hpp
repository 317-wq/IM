/**
 * @file gateway_server.hpp
 * @brief HTTP + WebSocket 网关声明。
 */

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <brpc/channel.h>
#include <httplib.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "channel.hpp"
#include "redis.hpp"
#include "transmit.pb.h"

namespace im {

/**
 * @brief 网关：HTTP 接口 + WebSocket 鉴权/推送。
 */
class GatewayServer {
public:
    using WsServer = websocketpp::server<websocketpp::config::asio>;

    GatewayServer(int http_port,
                  int ws_port,
                  std::shared_ptr<sw::redis::Redis> redis,
                  ServiceManager::Ptr channels,
                  std::string user_service,
                  std::string friend_service,
                  std::string transmit_service,
                  std::string message_service,
                  std::string file_service);

    void start();

private:
    template <bool NeedAuth, typename ReqT, typename RspT, typename Call>
    void handle(const std::string& service,
                const httplib::Request& req, httplib::Response& res, Call call);

    /// WebSocket 首帧鉴权：session_id -> uid，绑定连接
    void onWsMessage(websocketpp::connection_hdl hdl, WsServer::message_ptr msg);

    /// 连接断开：移除 uid 映射
    void onWsClose(websocketpp::connection_hdl hdl);

    /// 把新消息推给在线目标
    void pushMessage(const SendMessageRsp& response);

    httplib::Server http_;
    WsServer ws_;
    int ws_port_;
    std::thread http_thread_;

    std::shared_ptr<SessionStore> sessions_;
    ServiceManager::Ptr channels_;
    std::string user_service_;
    std::string friend_service_;
    std::string transmit_service_;
    std::string message_service_;
    std::string file_service_;

    std::mutex ws_mutex_;
    std::unordered_map<std::string, WsServer::connection_ptr> uid_conns_;
    std::unordered_map<WsServer::connection_ptr, std::string> conn_uids_;
};

} // namespace im
