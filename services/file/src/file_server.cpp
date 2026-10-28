/**
 * @file file_server.cpp
 * @brief 文件服务实现与程序入口。
 *
 * 职责：
 *   1. 上传：生成 uuid，按 id 落盘；
 *   2. 下载：校验 id 格式后读盘返回；
 *   3. 启动后把自身注册到 etcd，供其他服务按服务名发现。
 */

#include "file_server.hpp"

#include <cctype>
#include <filesystem>

#include <brpc/server.h>
#include <gflags/gflags.h>

#include "etcd.hpp"
#include "logger.hpp"
#include "utils.hpp"

DEFINE_bool(run_mode, false, "运行模式：false-调试（控制台），true-发布（文件）");
DEFINE_string(log_dir, "logs/file", "发布模式下的日志目录（按天生成 YYYY-MM-DD.log）");
DEFINE_int32(log_level, 0, "日志等级：0 trace / 1 debug / 2 info / 3 warn / 4 error");

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd 地址");
DEFINE_string(service_base, "/im/service", "服务注册根目录");
DEFINE_string(service_name, "file", "本服务名称");
DEFINE_string(instance_id, "", "实例标识，空则使用 advertise_host");
DEFINE_string(advertise_host, "127.0.0.1:10001", "对外访问地址 host:port");
DEFINE_int32(listen_port, 10001, "RPC 监听端口");
DEFINE_int32(rpc_threads, 4, "RPC IO 线程数");

DEFINE_string(storage_dir, "./data/files", "文件存储目录");

namespace im {

namespace {

/// 单文件大小上限 64MB，避免一次请求把内存打满
constexpr size_t kMaxFileSize = 64 * 1024 * 1024;

/**
 * @brief 校验文件 id 是否为 36 字符 uuid：只放行安全字符，防止路径穿越。
 */
bool isValidFileId(const std::string& id) {
    if (id.size() != 36) {
        return false;
    }
    for (size_t i = 0; i < id.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (id[i] != '-') {
                return false;
            }
        } else if (!std::isxdigit(static_cast<unsigned char>(id[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

/**
 * @brief 实现构造：创建存储目录并统一补上结尾 '/'。
 */
FileServiceImpl::FileServiceImpl(std::string storage_dir)
    : storage_dir_(std::move(storage_dir)) {
    std::filesystem::create_directories(storage_dir_);
    if (storage_dir_.back() != '/') {
        storage_dir_.push_back('/');
    }
}

/**
 * @brief 实现上传：检查大小 -> 生成 uuid -> 写盘 -> 返回 id。
 */
void FileServiceImpl::PutFile(google::protobuf::RpcController* /*controller*/,
                              const PutFileRequest* request,
                              PutFileResponse* response,
                              google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    if (request->content().size() > kMaxFileSize) {
        response->set_success(false);
        response->set_errmsg("file too large");
        return;
    }

    const std::string file_id = uuid();
    if (!writeFile(pathFor(file_id), request->content())) {
        IM_ERROR("write file {} failed", file_id);
        response->set_success(false);
        response->set_errmsg("write file failed");
        return;
    }

    response->set_success(true);
    response->set_file_id(file_id);
    response->set_size(static_cast<int64_t>(request->content().size()));
    IM_INFO("put file {} name={} size={}", file_id, request->file_name(),
            request->content().size());
}

/**
 * @brief 实现下载：校验 id -> 读盘 -> 返回内容。
 */
void FileServiceImpl::GetFile(google::protobuf::RpcController* /*controller*/,
                              const GetFileRequest* request,
                              GetFileResponse* response,
                              google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    if (!isValidFileId(request->file_id())) {
        response->set_success(false);
        response->set_errmsg("invalid file id");
        return;
    }

    std::string content;
    if (!readFile(pathFor(request->file_id()), content)) {
        IM_ERROR("read file {} failed", request->file_id());
        response->set_success(false);
        response->set_errmsg("read file failed");
        return;
    }

    response->set_success(true);
    response->set_content(std::move(content));
}

/**
 * @brief 实现批量下载：逐个校验并读取，失败即整体失败。
 */
void FileServiceImpl::GetMultiFile(google::protobuf::RpcController* /*controller*/,
                                   const GetMultiFileRequest* request,
                                   GetMultiFileResponse* response,
                                   google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    for (const auto& file_id : request->file_id()) {
        if (!isValidFileId(file_id)) {
            response->set_success(false);
            response->set_errmsg("invalid file id");
            return;
        }
        std::string content;
        if (!readFile(pathFor(file_id), content)) {
            response->set_success(false);
            response->set_errmsg("read file failed");
            return;
        }
        (*response->mutable_files())[file_id] = std::move(content);
    }

    response->set_success(true);
}

/**
 * @brief 实现路径拼接：存储目录 + 文件 id。
 */
std::string FileServiceImpl::pathFor(const std::string& file_id) const {
    return storage_dir_ + file_id;
}

} // namespace im

/**
 * @brief 程序入口：解析参数 -> 起 RPC 服务 -> 注册到 etcd -> 阻塞运行。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    im::init_logger(FLAGS_run_mode, FLAGS_log_dir, FLAGS_log_level);

    brpc::Server server;
    im::FileServiceImpl service(FLAGS_storage_dir);

    if (server.AddService(&service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        IM_FATAL("add file service failed");
        return -1;
    }

    brpc::ServerOptions options;
    options.num_threads = FLAGS_rpc_threads;
    if (server.Start(FLAGS_listen_port, &options) != 0) {
        IM_FATAL("start file server on port {} failed", FLAGS_listen_port);
        return -1;
    }

    const std::string instance =
        FLAGS_instance_id.empty() ? FLAGS_advertise_host : FLAGS_instance_id;
    im::Registry registry(FLAGS_etcd_host, FLAGS_service_base);
    if (!registry.registerService(FLAGS_service_name, instance, FLAGS_advertise_host)) {
        IM_FATAL("register service {} failed", FLAGS_service_name);
        return -1;
    }

    IM_INFO("file_server listening on 0.0.0.0:{}, storage={}", FLAGS_listen_port,
            FLAGS_storage_dir);
    server.RunUntilAskedToQuit();
    return 0;
}
