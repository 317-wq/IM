/**
 * @file redis.hpp
 * @brief Redis 客户端与会话/在线状态封装声明（实现见 common/src/redis.cpp）。
 *
 * 只封装 M3 需要的三件事：连接创建、会话映射、在线标记；
 * 后续里程碑按需增加其他数据结构。
 */

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include <sw/redis++/redis++.h>

// hiredis 的 REDIS_REPLY_* 宏与 brpc/redis_reply.h 中的同名枚举冲突，
// 本项目只用 redis++ 的 C++ 接口，包含后立即取消这些宏，避免 brpc 头文件解析失败。
#undef REDIS_REPLY_STRING
#undef REDIS_REPLY_ARRAY
#undef REDIS_REPLY_INTEGER
#undef REDIS_REPLY_NIL
#undef REDIS_REPLY_STATUS
#undef REDIS_REPLY_ERROR

namespace im {

/**
 * @brief Redis 客户端工厂。
 */
class RedisClientFactory {
public:
    /**
     * @brief 创建 Redis 客户端。
     * @param host       地址
     * @param port       端口
     * @param db         库号
     * @param keep_alive 是否保持长连接
     */
    static std::shared_ptr<sw::redis::Redis> create(const std::string& host,
                                                    int port,
                                                    int db,
                                                    bool keep_alive);
};

/**
 * @brief 登录会话存储：session_id -> user_id，带过期时间。
 */
class SessionStore {
public:
    /**
     * @brief 构造。
     * @param redis 已连接的 Redis 客户端
     * @param ttl   会话有效期
     */
    SessionStore(std::shared_ptr<sw::redis::Redis> redis, std::chrono::seconds ttl);

    /**
     * @brief 写入会话。
     */
    void save(const std::string& session_id, const std::string& user_id);

    /**
     * @brief 查询会话对应的用户；不存在返回空。
     */
    std::optional<std::string> userId(const std::string& session_id);

    /**
     * @brief 删除会话（登出或过期清理）。
     */
    void remove(const std::string& session_id);

private:
    std::shared_ptr<sw::redis::Redis> redis_;
    std::chrono::seconds ttl_;
};

/**
 * @brief 在线状态存储：user_id -> session_id，用于单点登录。
 */
class OnlineStore {
public:
    /**
     * @brief 构造。
     * @param redis 已连接的 Redis 客户端
     * @param ttl   在线标记有效期，与会话保持一致
     */
    OnlineStore(std::shared_ptr<sw::redis::Redis> redis, std::chrono::seconds ttl);

    /**
     * @brief 标记用户在线。
     */
    void save(const std::string& user_id, const std::string& session_id);

    /**
     * @brief 查询用户当前的会话；不在线返回空。
     */
    std::optional<std::string> sessionId(const std::string& user_id);

    /**
     * @brief 判断用户是否在线。
     */
    bool exists(const std::string& user_id);

    /**
     * @brief 清除在线标记。
     */
    void remove(const std::string& user_id);

private:
    std::shared_ptr<sw::redis::Redis> redis_;
    std::chrono::seconds ttl_;
};

} // namespace im
