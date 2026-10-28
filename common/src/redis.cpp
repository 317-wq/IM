/**
 * @file redis.cpp
 * @brief Redis 客户端、会话与在线状态封装实现。
 */

#include "redis.hpp"

namespace im {

namespace {

/// 会话 key 前缀
constexpr const char* kSessionPrefix = "im:session:";

/// 在线标记 key 前缀
constexpr const char* kOnlinePrefix = "im:online:";

} // namespace

/**
 * @brief 实现客户端创建：连接参数全部显式传入，避免依赖默认值。
 */
std::shared_ptr<sw::redis::Redis> RedisClientFactory::create(const std::string& host,
                                                             int port,
                                                             int db,
                                                             bool keep_alive) {
    sw::redis::ConnectionOptions options;
    options.host = host;
    options.port = port;
    options.db = db;
    options.keep_alive = keep_alive;
    return std::make_shared<sw::redis::Redis>(options);
}

/**
 * @brief 实现会话构造。
 */
SessionStore::SessionStore(std::shared_ptr<sw::redis::Redis> redis,
                           std::chrono::seconds ttl)
    : redis_(std::move(redis)), ttl_(ttl) {}

/**
 * @brief 实现会话写入：带 TTL。
 */
void SessionStore::save(const std::string& session_id, const std::string& user_id) {
    redis_->set(kSessionPrefix + session_id, user_id, ttl_);
}

/**
 * @brief 实现会话查询。
 */
std::optional<std::string> SessionStore::userId(const std::string& session_id) {
    auto value = redis_->get(kSessionPrefix + session_id);
    if (!value) {
        return std::nullopt;
    }
    return *value;
}

/**
 * @brief 实现会话删除。
 */
void SessionStore::remove(const std::string& session_id) {
    redis_->del(kSessionPrefix + session_id);
}

/**
 * @brief 实现在线状态构造。
 */
OnlineStore::OnlineStore(std::shared_ptr<sw::redis::Redis> redis,
                         std::chrono::seconds ttl)
    : redis_(std::move(redis)), ttl_(ttl) {}

/**
 * @brief 实现在线标记写入。
 */
void OnlineStore::save(const std::string& user_id, const std::string& session_id) {
    redis_->set(kOnlinePrefix + user_id, session_id, ttl_);
}

/**
 * @brief 实现在线状态查询。
 */
std::optional<std::string> OnlineStore::sessionId(const std::string& user_id) {
    auto value = redis_->get(kOnlinePrefix + user_id);
    if (!value) {
        return std::nullopt;
    }
    return *value;
}

/**
 * @brief 实现在线判断。
 */
bool OnlineStore::exists(const std::string& user_id) {
    return redis_->exists(kOnlinePrefix + user_id) > 0;
}

/**
 * @brief 实现在线标记删除。
 */
void OnlineStore::remove(const std::string& user_id) {
    redis_->del(kOnlinePrefix + user_id);
}

} // namespace im
