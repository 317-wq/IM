/**
 * @file es.hpp
 * @brief Elasticsearch 用户索引封装声明（实现见 common/src/es.cpp）。
 *
 * 只承担 M4 需要的用户检索：建索引、写入/更新用户、按关键字搜索。
 * 使用 cpr 直接调用 ES HTTP API，避免依赖 elasticlient 的接口细节。
 */

#pragma once

#include <string>
#include <vector>

namespace im {

/// 用户索引中的一条文档
struct ESUserDoc {
    std::string user_id;
    std::string account;
    std::string nickname;
};

/// 批量索引的一条消息
struct ESMessageIndex {
    std::string message_id;
    std::string session_id;
    std::string sender_id;
    std::string content;
    int64_t created_at;
};

/// 消息索引搜索结果（只取 id，详情回 MySQL 查）
struct ESMessageDoc {
    std::string message_id;
};

/**
 * @brief 用户索引客户端。
 */
class ESClient {
public:
    /**
     * @param base_url ES 地址，如 http://127.0.0.1:9200
     * @param index    索引名，如 im_user
     */
    ESClient(std::string base_url, std::string index);

    /**
     * @brief 确保索引存在（不存在则创建 mapping）。
     * @return 成功或已存在返回 true
     */
    bool ensureIndex();

    /**
     * @brief 写入/更新一个用户文档（以 user_id 作为文档 id）。
     */
    bool upsertUser(const std::string& user_id,
                    const std::string& account,
                    const std::string& nickname);

    /**
     * @brief 按关键字模糊搜索 account / nickname。
     * @param key             搜索关键字
     * @param exclude_user_id 排除的用户（通常是搜索者自己）
     * @param limit           最多返回条数
     */
    std::vector<ESUserDoc> searchUsers(const std::string& key,
                                       const std::string& exclude_user_id,
                                       int limit);

    /**
     * @brief 确保消息索引存在（message_id/session_id/sender_id/content/created_at）。
     */
    bool ensureMessageIndex();

    /**
     * @brief 写入一条消息文档。
     */
    bool indexMessage(const std::string& message_id,
                      const std::string& session_id,
                      const std::string& sender_id,
                      const std::string& content,
                      int64_t created_at);

    /**
     * @brief 批量写入消息文档（_bulk，NDJSON）。
     */
    bool bulkIndexMessages(const std::vector<ESMessageIndex>& docs);

    /**
     * @brief 在指定会话内按关键字搜索消息。
     */
    std::vector<ESMessageDoc> searchMessages(const std::string& session_id,
                                             const std::string& key,
                                             int limit);

private:
    std::string base_url_;
    std::string index_;
};

} // namespace im
