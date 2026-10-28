/**
 * @file file_client.cpp
 * @brief M1 文件服务验收客户端。
 *
 * 通过 etcd 发现 file 服务，然后执行上传、下载或批量下载：
 *   ./file_client -op=put -file=./docs/开发计划.md
 *   ./file_client -op=get -file_id=<id> -out=/tmp/out.md
 *   ./file_client -op=multi -file_id=<id1>,<id2>
 *   ./file_client -op=put -file=./conf/file.conf -repeat=10   # 多节点轮询测试
 */

#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <brpc/channel.h>
#include <gflags/gflags.h>

#include "channel.hpp"
#include "etcd.hpp"
#include "file.pb.h"
#include "logger.hpp"
#include "utils.hpp"

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd 地址");
DEFINE_string(service_base, "/im/service", "服务注册根目录");
DEFINE_string(service_name, "file", "目标服务名");
DEFINE_string(op, "put", "操作：put 上传 / get 下载 / multi 批量下载");
DEFINE_string(file, "", "上传时的本地文件路径");
DEFINE_string(file_id, "", "下载时的文件 id；multi 时用逗号分隔多个");
DEFINE_string(out, "", "下载时的输出路径");
DEFINE_int32(repeat, 1, "重复执行次数，用于观察多节点轮询");
DEFINE_int32(rpc_timeout_ms, 30000, "RPC 超时（毫秒）");

DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/file_client", "发布模式下的日志目录");
DEFINE_int32(log_level, 0, "日志等级");

namespace {

/**
 * @brief 每次调用前重新选节点，保证多节点场景下能轮询到不同实例。
 */
im::ServiceChannel::ChannelPtr pickChannel(im::ServiceManager& manager) {
    auto channel = manager.choose(FLAGS_service_name);
    if (!channel) {
        IM_ERROR("no available node for service {}", FLAGS_service_name);
    }
    return channel;
}

/**
 * @brief 上传流程：读本地文件 -> 调 PutFile -> 打印 file_id。
 */
int doPut(im::ServiceManager& manager) {
    std::string body;
    if (!im::readFile(FLAGS_file, body)) {
        IM_ERROR("read local file {} failed", FLAGS_file);
        return 1;
    }

    const std::string file_name =
        std::filesystem::path(FLAGS_file).filename().string();

    for (int i = 0; i < FLAGS_repeat; ++i) {
        auto channel = pickChannel(manager);
        if (!channel) {
            return 1;
        }
        im::FileService_Stub stub(channel.get());

        im::PutFileRequest request;
        request.set_request_id(im::uuid());
        request.set_file_name(file_name);
        request.set_content(body);

        im::PutFileResponse response;
        brpc::Controller cntl;
        cntl.set_timeout_ms(FLAGS_rpc_timeout_ms);
        stub.PutFile(&cntl, &request, &response, nullptr);

        if (cntl.Failed() || !response.success()) {
            IM_ERROR("put file failed: {}",
                     cntl.Failed() ? cntl.ErrorText() : response.errmsg());
            return 1;
        }
        std::printf("file_id=%s size=%ld\n", response.file_id().c_str(), response.size());
    }
    return 0;
}

/**
 * @brief 下载流程：调 GetFile -> 可选写本地 -> 打印大小。
 */
int doGet(im::ServiceManager& manager) {
    for (int i = 0; i < FLAGS_repeat; ++i) {
        auto channel = pickChannel(manager);
        if (!channel) {
            return 1;
        }
        im::FileService_Stub stub(channel.get());

        im::GetFileRequest request;
        request.set_request_id(im::uuid());
        request.set_file_id(FLAGS_file_id);

        im::GetFileResponse response;
        brpc::Controller cntl;
        cntl.set_timeout_ms(FLAGS_rpc_timeout_ms);
        stub.GetFile(&cntl, &request, &response, nullptr);

        if (cntl.Failed() || !response.success()) {
            IM_ERROR("get file failed: {}",
                     cntl.Failed() ? cntl.ErrorText() : response.errmsg());
            return 1;
        }

        if (i == FLAGS_repeat - 1 && !FLAGS_out.empty() &&
            !im::writeFile(FLAGS_out, response.content())) {
            IM_ERROR("write {} failed", FLAGS_out);
            return 1;
        }

        std::printf("file_id=%s size=%zu\n", FLAGS_file_id.c_str(),
                    response.content().size());
    }
    return 0;
}

/**
 * @brief 批量下载流程：把逗号分隔的 id 拆开，逐个请求并打印大小。
 */
int doMulti(im::ServiceManager& manager) {
    std::vector<std::string> ids;
    std::stringstream ss(FLAGS_file_id);
    std::string id;
    while (std::getline(ss, id, ',')) {
        if (!id.empty()) {
            ids.push_back(id);
        }
    }

    for (int i = 0; i < FLAGS_repeat; ++i) {
        auto channel = pickChannel(manager);
        if (!channel) {
            return 1;
        }
        im::FileService_Stub stub(channel.get());

        im::GetMultiFileRequest request;
        request.set_request_id(im::uuid());
        for (const auto& item : ids) {
            request.add_file_id(item);
        }

        im::GetMultiFileResponse response;
        brpc::Controller cntl;
        cntl.set_timeout_ms(FLAGS_rpc_timeout_ms);
        stub.GetMultiFile(&cntl, &request, &response, nullptr);

        if (cntl.Failed() || !response.success()) {
            IM_ERROR("multi get failed: {}",
                     cntl.Failed() ? cntl.ErrorText() : response.errmsg());
            return 1;
        }

        for (const auto& item : response.files()) {
            std::printf("file_id=%s size=%zu\n", item.first.c_str(),
                        item.second.size());
        }
    }
    return 0;
}

} // namespace

/**
 * @brief 程序入口：发现服务 -> 按 op 执行。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    im::init_logger(FLAGS_run_mode, FLAGS_log_dir, FLAGS_log_level);

    im::ServiceManager manager(FLAGS_service_base);
    manager.follow(FLAGS_service_name);

    im::Discovery discovery(
        FLAGS_etcd_host, FLAGS_service_base,
        [&manager](const std::string& key, const std::string& host) {
            manager.onServiceOnline(key, host);
        },
        [&manager](const std::string& key, const std::string& host) {
            manager.onServiceOffline(key, host);
        });

    if (FLAGS_op == "put") {
        return doPut(manager);
    }
    if (FLAGS_op == "get") {
        return doGet(manager);
    }
    if (FLAGS_op == "multi") {
        return doMulti(manager);
    }

    IM_ERROR("unknown op {}", FLAGS_op);
    return 1;
}
