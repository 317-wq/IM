/**
 * @file es.cpp
 * @brief Elasticsearch 用户索引实现：cpr + nlohmann/json 调用 HTTP API。
 */

#include "es.hpp"

#include <nlohmann/json.hpp>

#include <cpr/cpr.h>

#include "logger.hpp"

namespace im {

using json = nlohmann::json;

namespace {

/**
 * @brief 判断 HTTP 状态码是否成功（2xx）。
 */
bool ok(int status) {
    return status >= 200 && status < 300;
}

} // namespace

/**
 * @brief 实现构造：保存 ES 地址与索引名。
 */
ESClient::ESClient(std::string base_url, std::string index)
    : base_url_(std::move(base_url)), index_(std::move(index)) {}

/**
 * @brief 实现建索引：keyword 类型的 user_id/account/nickname，支持通配符查询。
 */
bool ESClient::ensureIndex() {
    const json mapping = {
        {"mappings", {{"properties", {
            {"user_id", {{"type", "keyword"}}},
            {"account", {{"type", "keyword"}}},
            {"nickname", {{"type", "keyword"}}},
        }}}},
    };

    auto response = cpr::Put(
        cpr::Url{base_url_ + "/" + index_},
        cpr::Body{mapping.dump()},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Timeout{3000});

    if (ok(response.status_code)) {
        IM_INFO("es index {} created", index_);
        return true;
    }
    if (response.status_code == 400 &&
        response.text.find("resource_already_exists_exception") != std::string::npos) {
        IM_INFO("es index {} already exists", index_);
        return true;
    }
    IM_ERROR("create es index {} failed: {} {}", index_, response.status_code,
             response.text);
    return false;
}

/**
 * @brief 实现写入：PUT /<index>/_doc/<user_id>?refresh=true。
 */
bool ESClient::upsertUser(const std::string& user_id,
                          const std::string& account,
                          const std::string& nickname) {
    const json doc = {{"user_id", user_id}, {"account", account}, {"nickname", nickname}};

    auto response = cpr::Put(
        cpr::Url{base_url_ + "/" + index_ + "/_doc/" + user_id + "?refresh=true"},
        cpr::Body{doc.dump()},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Timeout{3000});

    if (!ok(response.status_code)) {
        IM_WARN("es upsert user {} failed: {} {}", user_id, response.status_code,
                response.text);
        return false;
    }
    return true;
}

/**
 * @brief 实现消息索引创建：字段均为 keyword/long，便于精确过滤与通配符搜索。
 */
bool ESClient::ensureMessageIndex() {
    const json mapping = {
        {"mappings", {{"properties", {
            {"message_id", {{"type", "keyword"}}},
            {"session_id", {{"type", "keyword"}}},
            {"sender_id", {{"type", "keyword"}}},
            {"content", {{"type", "keyword"}}},
            {"created_at", {{"type", "long"}}},
        }}}},
    };

    auto response = cpr::Put(
        cpr::Url{base_url_ + "/" + index_},
        cpr::Body{mapping.dump()},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Timeout{3000});

    if (ok(response.status_code) ||
        (response.status_code == 400 &&
         response.text.find("resource_already_exists_exception") != std::string::npos)) {
        IM_INFO("es message index {} ready", index_);
        return true;
    }
    IM_ERROR("create es message index {} failed: {}", index_, response.status_code);
    return false;
}

/**
 * @brief 实现消息写入：以 message_id 作为文档 id。
 */
bool ESClient::indexMessage(const std::string& message_id,
                            const std::string& session_id,
                            const std::string& sender_id,
                            const std::string& content,
                            int64_t created_at) {
    const json doc = {
        {"message_id", message_id},
        {"session_id", session_id},
        {"sender_id", sender_id},
        {"content", content},
        {"created_at", created_at},
    };

    auto response = cpr::Put(
        cpr::Url{base_url_ + "/" + index_ + "/_doc/" + message_id},
        cpr::Body{doc.dump()},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Timeout{3000});

    if (!ok(response.status_code)) {
        IM_WARN("es index message {} failed: {} {}", message_id, response.status_code,
                response.text);
        return false;
    }
    return true;
}

/**
 * @brief 实现消息搜索：session_id 精确匹配 + content 通配符。
 */
std::vector<ESMessageDoc> ESClient::searchMessages(const std::string& session_id,
                                                   const std::string& key,
                                                   int limit) {
    const json query = {
        {"size", limit},
        {"query", {{"bool", {
            {"must", json::array({
                {{"term", {{"session_id", session_id}}}},
                {{"wildcard", {{"content", "*" + key + "*"}}}},
            })},
        }}}},
    };

    auto response = cpr::Post(
        cpr::Url{base_url_ + "/" + index_ + "/_search"},
        cpr::Body{query.dump()},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Timeout{3000});

    std::vector<ESMessageDoc> result;
    if (!ok(response.status_code)) {
        IM_ERROR("es search messages failed: {} {}", response.status_code, response.text);
        return result;
    }
    try {
        const json body = json::parse(response.text);
        for (const auto& hit : body.at("hits").at("hits")) {
            result.push_back(ESMessageDoc{hit.at("_source").value("message_id", "")});
        }
    } catch (const std::exception& e) {
        IM_ERROR("parse message search result failed: {}", e.what());
    }
    return result;
}

/**
 * @brief 实现搜索：account / nickname 通配符匹配，排除指定用户。
 */
std::vector<ESUserDoc> ESClient::searchUsers(const std::string& key,
                                             const std::string& exclude_user_id,
                                             int limit) {
    json should = json::array();
    should.push_back({{"wildcard", {{"account", "*" + key + "*"}}}});
    should.push_back({{"wildcard", {{"nickname", "*" + key + "*"}}}});

    json query = {
        {"size", limit},
        {"query", {{"bool", {
            {"should", should},
            {"minimum_should_match", 1},
        }}}},
    };
    if (!exclude_user_id.empty()) {
        query["query"]["bool"]["must_not"] =
            json::array({{{"term", {{"user_id", exclude_user_id}}}}});
    }

    auto response = cpr::Post(
        cpr::Url{base_url_ + "/" + index_ + "/_search"},
        cpr::Body{query.dump()},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Timeout{3000});

    std::vector<ESUserDoc> result;
    if (!ok(response.status_code)) {
        IM_ERROR("es search {} failed: {} {}", key, response.status_code, response.text);
        return result;
    }

    try {
        const json body = json::parse(response.text);
        for (const auto& hit : body.at("hits").at("hits")) {
            const auto& source = hit.at("_source");
            result.push_back(ESUserDoc{
                source.value("user_id", ""),
                source.value("account", ""),
                source.value("nickname", ""),
            });
        }
    } catch (const std::exception& e) {
        IM_ERROR("parse es search result failed: {}", e.what());
    }
    return result;
}

} // namespace im
