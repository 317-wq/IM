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

private:
    std::string base_url_;
    std::string index_;
};

} // namespace im
