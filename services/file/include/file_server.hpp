/**
 * @file file_server.hpp
 * @brief 文件服务实现类声明（实现见 services/file/src/file_server.cpp）。
 *
 * 文件按 uuid 命名落在本地磁盘，服务端只认 uuid，避免客户端传路径造成目录穿越。
 */

#pragma once

#include <string>

#include "file.pb.h"

namespace im {

/**
 * @brief 文件服务实现：上传写入磁盘，下载按 id 读回。
 */
class FileServiceImpl final : public FileService {
public:
    /**
     * @brief 构造并确保存储目录存在。
     * @param storage_dir 文件存储目录，如 ./data/files
     */
    explicit FileServiceImpl(std::string storage_dir);

    /**
     * @brief 上传文件：生成 uuid 作为文件 id，写入磁盘。
     */
    void PutFile(google::protobuf::RpcController* controller,
                 const PutFileRequest* request,
                 PutFileResponse* response,
                 google::protobuf::Closure* done) override;

    /**
     * @brief 下载单个文件：校验 id 后读取内容返回。
     */
    void GetFile(google::protobuf::RpcController* controller,
                 const GetFileRequest* request,
                 GetFileResponse* response,
                 google::protobuf::Closure* done) override;

    /**
     * @brief 批量下载：逐个读取，任意一个失败则整体失败。
     */
    void GetMultiFile(google::protobuf::RpcController* controller,
                      const GetMultiFileRequest* request,
                      GetMultiFileResponse* response,
                      google::protobuf::Closure* done) override;

private:
    /**
     * @brief 拼出文件 id 对应的磁盘路径。
     */
    std::string pathFor(const std::string& file_id) const;

    std::string storage_dir_;
};

} // namespace im
