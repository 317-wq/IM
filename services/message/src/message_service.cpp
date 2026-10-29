/**
 * @file message_service.cpp
 * @brief 消息查询 RPC 服务实现。
 */

#include "message_service.hpp"

#include <algorithm>

#include "logger.hpp"
#include "user.pb.h"

namespace im {

/**
 * @brief 实现构造。
 */
MessageServiceImpl::MessageServiceImpl(std::shared_ptr<odb::core::database> db,
                                       std::shared_ptr<ESClient> es,
                                       ServiceManager::Ptr channels,
                                       std::string user_service)
    : db_(std::move(db)),
      messages_(std::make_shared<MessageDao>(db_)),
      es_(std::move(es)),
      channels_(std::move(channels)),
      user_service_(std::move(user_service)) {}

/**
 * @brief 实现批量拉取用户信息（每 100 个一组）。
 */
bool MessageServiceImpl::fetchUsers(const std::string& request_id,
                                    const std::vector<std::string>& user_ids,
                                    std::unordered_map<std::string, UserInfo>& users) {
    constexpr size_t kBatch = 100;
    for (size_t begin = 0; begin < user_ids.size(); begin += kBatch) {
        const size_t end = std::min(begin + kBatch, user_ids.size());
        auto channel = channels_->choose(user_service_);
        if (!channel) {
            return false;
        }

        GetMultiUserInfoReq request;
        request.set_request_id(request_id);
        for (size_t i = begin; i < end; ++i) {
            request.add_user_ids(user_ids[i]);
        }

        GetMultiUserInfoRsp response;
        UserService_Stub stub(channel.get());
        brpc::Controller cntl;
        stub.GetMultiUserInfo(&cntl, &request, &response, nullptr);
        if (cntl.Failed() || !response.success()) {
            return false;
        }
        for (const auto& item : response.users()) {
            users[item.first] = item.second;
        }
    }
    return true;
}

/**
 * @brief 实现消息转换：文本填 content，图片/文件/语音填 file 元信息。
 */
void MessageServiceImpl::buildMessage(
    const Message& message,
    const std::unordered_map<std::string, UserInfo>& users,
    MessageInfo* info) {
    info->set_message_id(message.message_id());
    info->set_chat_session_id(message.session_id());
    info->set_timestamp(boost::posix_time::to_time_t(message.created_at()));

    auto it = users.find(message.sender_id());
    if (it != users.end()) {
        info->mutable_sender()->CopyFrom(it->second);
    } else {
        info->mutable_sender()->set_user_id(message.sender_id());
    }

    auto* content = info->mutable_message();
    switch (message.type()) {
        case MessageType::STRING:
            content->set_message_type(MessageType::STRING);
            content->mutable_string_message()->set_content(message.content());
            break;
        case MessageType::IMAGE:
            content->set_message_type(MessageType::IMAGE);
            content->mutable_image_message()->set_file_id(message.file_id());
            break;
        case MessageType::FILE:
            content->set_message_type(MessageType::FILE);
            content->mutable_file_message()->set_file_id(message.file_id());
            content->mutable_file_message()->set_file_name(message.file_name());
            content->mutable_file_message()->set_file_size(message.file_size());
            break;
        case MessageType::SPEECH:
            content->set_message_type(MessageType::SPEECH);
            content->mutable_speech_message()->set_file_id(message.file_id());
            break;
        default:
            break;
    }
}

/**
 * @brief 实现历史消息：按时间范围查询并补发送者信息。
 */
void MessageServiceImpl::GetHistory(google::protobuf::RpcController*,
                                    const GetHistoryMsgReq* request,
                                    GetHistoryMsgRsp* response,
                                    google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    // MySQL TIMESTAMP 不接受 1970-01-01 00:00:00（时区换算后会越界），
    // start_time 为 0 时从 2000-01-01 开始查，足够覆盖本项目的消息。
    const auto start = request->start_time() <= 0
                           ? boost::posix_time::time_from_string("2000-01-01 00:00:00")
                           : boost::posix_time::from_time_t(request->start_time());
    auto rows = messages_->range(request->chat_session_id(), start,
                                 boost::posix_time::from_time_t(request->end_time()));
    std::vector<std::string> ids;
    for (const auto& row : rows) {
        ids.push_back(row.sender_id());
    }
    std::unordered_map<std::string, UserInfo> users;
    fetchUsers(request->request_id(), ids, users);

    for (const auto& row : rows) {
        buildMessage(row, users, response->add_messages());
    }
    response->set_success(true);
}

/**
 * @brief 实现最近消息：按时间倒序取 N 条。
 */
void MessageServiceImpl::GetRecent(google::protobuf::RpcController*,
                                   const GetRecentMsgReq* request,
                                   GetRecentMsgRsp* response,
                                   google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    auto rows = messages_->listBySession(request->chat_session_id());
    std::sort(rows.begin(), rows.end(), [](const Message& a, const Message& b) {
        return a.created_at() > b.created_at();
    });
    if (request->count() > 0 && static_cast<size_t>(request->count()) < rows.size()) {
        rows.resize(static_cast<size_t>(request->count()));
    }

    std::vector<std::string> ids;
    for (const auto& row : rows) {
        ids.push_back(row.sender_id());
    }
    std::unordered_map<std::string, UserInfo> users;
    fetchUsers(request->request_id(), ids, users);

    for (const auto& row : rows) {
        buildMessage(row, users, response->add_messages());
    }
    response->set_success(true);
}

/**
 * @brief 实现搜索：ES 找 message_id -> MySQL 取详情。
 */
void MessageServiceImpl::Search(google::protobuf::RpcController*,
                                const MsgSearchReq* request,
                                MsgSearchRsp* response,
                                google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->set_request_id(request->request_id());

    auto hits = es_->searchMessages(request->chat_session_id(), request->search_key(), 50);
    std::vector<Message> rows;
    std::vector<std::string> ids;
    for (const auto& hit : hits) {
        auto message = messages_->findById(hit.message_id);
        if (message != nullptr) {
            ids.push_back(message->sender_id());
            rows.push_back(*message);
        }
    }
    std::sort(rows.begin(), rows.end(), [](const Message& a, const Message& b) {
        return a.created_at() < b.created_at();
    });

    std::unordered_map<std::string, UserInfo> users;
    fetchUsers(request->request_id(), ids, users);
    for (const auto& row : rows) {
        buildMessage(row, users, response->add_messages());
    }
    response->set_success(true);
}

} // namespace im
