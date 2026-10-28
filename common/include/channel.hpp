/**
 * @file channel.hpp
 * @brief brpc 信道管理声明（实现见 common/src/channel.cpp）。
 *
 * 两层结构：
 *   ServiceChannel —— 单个服务下所有节点的信道集合，负责轮询选节点；
 *   ServiceManager —— 按服务名管理 ServiceChannel，并把 etcd 的上下线事件转成增删节点。
 */

#pragma once

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <brpc/channel.h>

namespace im {

/**
 * @brief 单个服务的信道集合。
 *
 * 每个节点对应一个 brpc::Channel；调用时按轮询策略选一个，实现客户端负载均衡。
 */
class ServiceChannel {
public:
    using Ptr = std::shared_ptr<ServiceChannel>;
    using ChannelPtr = std::shared_ptr<brpc::Channel>;

    explicit ServiceChannel(std::string service_name);

    /**
     * @brief 节点上线：为 host 创建信道并加入集合。
     */
    void append(const std::string& host);

    /**
     * @brief 节点下线：移除并销毁 host 对应的信道。
     */
    void remove(const std::string& host);

    /**
     * @brief 轮询选择一个可用信道；没有节点时返回空指针。
     */
    ChannelPtr choose();

private:
    std::mutex mutex_;
    std::string service_name_;
    size_t index_{0};
    std::vector<ChannelPtr> channels_;
    std::unordered_map<std::string, ChannelPtr> hosts_;
};

/**
 * @brief 全局服务信道管理器。
 *
 * 只管理 follow() 声明过的服务；onServiceOnline/Offline 接收 etcd 的 key/host，
 * 解析出服务名后转发给对应的 ServiceChannel。
 */
class ServiceManager {
public:
    using Ptr = std::shared_ptr<ServiceManager>;

    /**
     * @param base_dir 服务根目录，用于从 key 中解析服务名，如 /im/service
     */
    explicit ServiceManager(std::string base_dir);

    /**
     * @brief 声明关注某个服务（未声明的服务即使上线也不建信道）。
     */
    void follow(const std::string& service);

    /**
     * @brief 节点上线事件：key 形如 /im/service/file/127.0.0.1:10001。
     */
    void onServiceOnline(const std::string& key, const std::string& host);

    /**
     * @brief 节点下线事件。
     */
    void onServiceOffline(const std::string& key, const std::string& host);

    /**
     * @brief 为指定服务选择一个可用信道；服务不存在或无节点时返回空指针。
     */
    ServiceChannel::ChannelPtr choose(const std::string& service);

private:
    /**
     * @brief 从 etcd key 中解析服务名：/im/service/file/xxx -> file。
     */
    std::string serviceName(const std::string& key) const;

    std::mutex mutex_;
    std::string base_dir_;
    std::set<std::string> followed_;
    std::unordered_map<std::string, ServiceChannel::Ptr> services_;
};

} // namespace im
