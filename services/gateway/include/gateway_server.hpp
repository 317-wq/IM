/**
 * @file gateway_server.hpp
 * @brief HTTP 网关声明（第一步：用户与消息查询路由）。
 */

#pragma once

#include <memory>
#include <string>

#include <brpc/channel.h>
#include <httplib.h>

#include "channel.hpp"
#include "redis.hpp"

namespace im {

/**
 * @brief HTTP 网关：protobuf 正文 + Redis 会话鉴权 + brpc 转发。
 */
class GatewayServer {
public:
    GatewayServer(int port,
                  std::shared_ptr<sw::redis::Redis> redis,
                  ServiceManager::Ptr channels,
                  std::string user_service,
                  std::string message_service);

    /// 阻塞运行
    void start();

private:
    /// 通用处理：解析 -> 鉴权 -> 转发 -> 返回 protobuf
    template <bool NeedAuth, typename ReqT, typename RspT, typename Call>
    void handle(const std::string& service,
                const httplib::Request& req, httplib::Response& res, Call call);

    httplib::Server http_;
    std::shared_ptr<SessionStore> sessions_;
    ServiceManager::Ptr channels_;
    std::string user_service_;
    std::string message_service_;
};

} // namespace im
