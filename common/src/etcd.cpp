/**
 * @file etcd.cpp
 * @brief etcd 注册中心封装实现：服务注册、服务发现与事件分发。
 */

#include "etcd.hpp"

#include "logger.hpp"

namespace im {

namespace {

/// 租约时长（秒）：进程异常退出后最多这么久节点自动摘除
constexpr int kLeaseTtlSeconds = 3;

} // namespace

/**
 * @brief 实现注册客户端：建立 etcd 客户端并申请保活租约。
 */
Registry::Registry(const std::string& etcd_host, const std::string& base_dir)
    : client_(std::make_shared<etcd::Client>(etcd_host)),
      base_dir_(base_dir) {
    keep_alive_ = client_->leasekeepalive(kLeaseTtlSeconds).get();
    lease_id_ = keep_alive_->Lease();
}

/**
 * @brief 实现析构：取消保活，租约到期后节点自动下线。
 */
Registry::~Registry() {
    if (keep_alive_) {
        keep_alive_->Cancel();
    }
}

/**
 * @brief 实现注册：写入 <base>/<service>/<instance> -> host，并绑定租约。
 */
bool Registry::registerService(const std::string& service,
                               const std::string& instance,
                               const std::string& host) {
    const std::string key = base_dir_ + "/" + service + "/" + instance;
    auto response = client_->put(key, host, lease_id_).get();
    if (!response.is_ok()) {
        IM_ERROR("register {} failed: {}", key, response.error_message());
        return false;
    }
    IM_INFO("register service ok: {} -> {}", key, host);
    return true;
}

/**
 * @brief 实现发现：先全量拉取，再启动递归 watch。
 */
Discovery::Discovery(const std::string& etcd_host,
                     const std::string& base_dir,
                     Callback on_put,
                     Callback on_del)
    : on_put_(std::move(on_put)),
      on_del_(std::move(on_del)),
      client_(std::make_shared<etcd::Client>(etcd_host)) {
    auto response = client_->ls(base_dir).get();
    if (!response.is_ok()) {
        IM_ERROR("list {} failed: {}", base_dir, response.error_message());
    } else {
        for (int i = 0; i < response.keys().size(); ++i) {
            if (on_put_) {
                on_put_(response.key(i), response.value(i).as_string());
            }
        }
        IM_INFO("discover {} nodes under {}", response.keys().size(), base_dir);
    }

    watcher_ = std::make_shared<etcd::Watcher>(
        *client_, base_dir,
        [this](const etcd::Response& resp) { handleEvent(resp); },
        true);
}

/**
 * @brief 实现析构：停止 watch。
 */
Discovery::~Discovery() {
    if (watcher_) {
        watcher_->Cancel();
    }
}

/**
 * @brief 实现事件分发：PUT 视为上线，DELETE 视为下线。
 */
void Discovery::handleEvent(const etcd::Response& response) {
    if (!response.is_ok()) {
        IM_ERROR("watch event error: {}", response.error_message());
        return;
    }

    for (const auto& event : response.events()) {
        if (event.event_type() == etcd::Event::EventType::PUT) {
            if (on_put_) {
                on_put_(event.kv().key(), event.kv().as_string());
            }
        } else if (event.event_type() == etcd::Event::EventType::DELETE_) {
            if (on_del_) {
                on_del_(event.prev_kv().key(), event.prev_kv().as_string());
            }
        }
    }
}

} // namespace im
