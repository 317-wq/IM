/**
 * @file etcd.hpp
 * @brief etcd 注册中心封装声明：服务注册 + 服务发现（实现见 common/src/etcd.cpp）。
 *
 * 约定：服务注册 key 为 <base_dir>/<service>/<instance>，value 为可访问的 host:port。
 * Discovery 先全量拉取一次，再 watch 目录变化，节点上下线通过回调通知业务层。
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>
#include <etcd/Response.hpp>
#include <etcd/Watcher.hpp>

namespace im {

/**
 * @brief 服务注册客户端。
 *
 * 构造时向 etcd 申请一个短租约并持续保活；注册的 key 绑定该租约，
 * 进程退出后租约到期，节点会自动从 etcd 消失。
 */
class Registry {
public:
    /**
     * @brief 连接 etcd。
     * @param etcd_host etcd 地址，如 http://127.0.0.1:2379
     * @param base_dir  服务根目录，如 /im/service
     */
    Registry(const std::string& etcd_host, const std::string& base_dir);

    ~Registry();

    /**
     * @brief 注册一个服务实例。
     * @param service  服务名，如 file
     * @param instance 实例标识，如 127.0.0.1:10001
     * @param host     对外访问地址 host:port
     * @return 写入成功返回 true
     */
    bool registerService(const std::string& service,
                         const std::string& instance,
                         const std::string& host);

private:
    std::shared_ptr<etcd::Client> client_;
    std::shared_ptr<etcd::KeepAlive> keep_alive_;
    int64_t lease_id_;
    std::string base_dir_;
};

/**
 * @brief 服务发现客户端。
 *
 * 构造时先 ls 全量拉取 base_dir 下已有节点，再 watch 增量变化；
 * 节点新增/删除时分别回调 on_put / on_del，参数为 (key, host)。
 */
class Discovery {
public:
    using Callback = std::function<void(const std::string&, const std::string&)>;

    /**
     * @brief 连接 etcd 并开始监听。
     * @param etcd_host etcd 地址
     * @param base_dir  服务根目录
     * @param on_put    节点上线回调
     * @param on_del    节点下线回调
     */
    Discovery(const std::string& etcd_host,
              const std::string& base_dir,
              Callback on_put,
              Callback on_del);

    ~Discovery();

private:
    /**
     * @brief 处理 watch 事件的内部回调：按事件类型分发到 on_put / on_del。
     */
    void handleEvent(const etcd::Response& response);

    Callback on_put_;
    Callback on_del_;
    std::shared_ptr<etcd::Client> client_;
    std::shared_ptr<etcd::Watcher> watcher_;
};

} // namespace im
