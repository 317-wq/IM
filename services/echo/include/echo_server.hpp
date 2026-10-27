/**
 * @file echo_server.hpp
 * @brief M0 回显服务类声明（实现见 echo_server.cpp）。
 *
 * 只声明服务实现类，保持头文件干净；后续里程碑的 user/friend 等服务
 * 也按「XxxService 声明放 .hpp、业务实现放 .cpp」的方式拆分。
 */

#pragma once

#include "echo.pb.h"

namespace im {

/**
 * @brief 回显服务实现类声明。
 *
 * 只做一件事：把客户端发来的文本加上 "echo: " 前缀回显，
 * 并带上服务端当前毫秒时间戳，方便调用方确认请求确实到达了服务端。
 */
class EchoServiceImpl final : public EchoService {
public:
    /**
     * @brief 处理一次 Echo 调用。
     * @param controller brpc 控制器（本服务不使用）
     * @param request    客户端请求（message 为待回显文本）
     * @param response   响应（message 为回显内容，server_time_ms 为服务端时间）
     * @param done       brpc 完成回调；实现里用 ClosureGuard 保证一定被调用
     */
    void Echo(google::protobuf::RpcController* controller,
              const EchoRequest* request,
              EchoResponse* response,
              google::protobuf::Closure* done) override;
};

} // namespace im
