/**
 * @file channel.cpp
 * @brief brpc 信道管理实现：单服务轮询选择 + 服务上下线维护。
 */

#include "channel.hpp"

#include "logger.hpp"

namespace im {

namespace {

/// RPC 超时（毫秒）；具体调用方可用 brpc::Controller 覆盖
constexpr int kRpcTimeoutMs = 3000;

/// 连接超时（毫秒）
constexpr int kConnectTimeoutMs = 1000;

/// 失败重试次数
constexpr int kMaxRetry = 2;

} // namespace

/**
 * @brief 实现构造：保存服务名。
 */
ServiceChannel::ServiceChannel(std::string service_name)
    : service_name_(std::move(service_name)) {}

/**
 * @brief 实现节点上线：创建 brpc 信道，初始化成功后加入轮询集合。
 */
void ServiceChannel::append(const std::string& host) {
    brpc::ChannelOptions options;
    options.protocol = "baidu_std";
    options.connect_timeout_ms = kConnectTimeoutMs;
    options.timeout_ms = kRpcTimeoutMs;
    options.max_retry = kMaxRetry;

    auto channel = std::make_shared<brpc::Channel>();
    if (channel->Init(host.c_str(), &options) != 0) {
        IM_ERROR("init channel for {}-{} failed", service_name_, host);
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (hosts_.count(host) != 0) {
        return;
    }
    hosts_.emplace(host, channel);
    channels_.push_back(std::move(channel));
    IM_INFO("service {} add node {}, total {}", service_name_, host, channels_.size());
}

/**
 * @brief 实现节点下线：从 hosts_ 与 channels_ 中同时移除。
 */
void ServiceChannel::remove(const std::string& host) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = hosts_.find(host);
    if (it == hosts_.end()) {
        return;
    }

    auto channel = it->second;
    for (auto vit = channels_.begin(); vit != channels_.end(); ++vit) {
        if (*vit == channel) {
            channels_.erase(vit);
            break;
        }
    }
    hosts_.erase(it);
    IM_INFO("service {} remove node {}, left {}", service_name_, host, channels_.size());
}

/**
 * @brief 实现轮询选择：按下标取模，均摊到每个节点。
 */
ServiceChannel::ChannelPtr ServiceChannel::choose() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (channels_.empty()) {
        IM_ERROR("no available node for service {}", service_name_);
        return nullptr;
    }
    const size_t index = index_++ % channels_.size();
    return channels_[index];
}

/**
 * @brief 实现构造：保存服务根目录。
 */
ServiceManager::ServiceManager(std::string base_dir)
    : base_dir_(std::move(base_dir)) {}

/**
 * @brief 实现关注声明：只对声明过的服务建立信道。
 */
void ServiceManager::follow(const std::string& service) {
    std::lock_guard<std::mutex> lock(mutex_);
    followed_.insert(service);
    IM_DEBUG("follow service {}", service);
}

/**
 * @brief 实现上线处理：解析服务名，必要时创建 ServiceChannel 并追加节点。
 */
void ServiceManager::onServiceOnline(const std::string& key, const std::string& host) {
    const std::string service = serviceName(key);
    if (service.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (followed_.count(service) == 0) {
        return;
    }

    auto it = services_.find(service);
    if (it == services_.end()) {
        it = services_.emplace(service, std::make_shared<ServiceChannel>(service)).first;
    }
    auto channel = it->second;
    channel->append(host);
}

/**
 * @brief 实现下线处理：找到对应服务并移除节点。
 */
void ServiceManager::onServiceOffline(const std::string& key, const std::string& host) {
    const std::string service = serviceName(key);
    if (service.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = services_.find(service);
    if (it == services_.end()) {
        return;
    }
    it->second->remove(host);
}

/**
 * @brief 实现选择：取出服务对应的 ServiceChannel 再轮询。
 */
ServiceChannel::ChannelPtr ServiceManager::choose(const std::string& service) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = services_.find(service);
    if (it == services_.end()) {
        IM_ERROR("service {} not found", service);
        return nullptr;
    }
    return it->second->choose();
}

/**
 * @brief 实现解析：去掉 base_dir 前缀和最后一段实例名，剩下的就是服务名。
 */
std::string ServiceManager::serviceName(const std::string& key) const {
    const std::string prefix = base_dir_ + "/";
    if (key.compare(0, prefix.size(), prefix) != 0) {
        return {};
    }

    const std::string rest = key.substr(prefix.size());
    const auto pos = rest.find('/');
    if (pos == std::string::npos) {
        return {};
    }
    return rest.substr(0, pos);
}

} // namespace im
