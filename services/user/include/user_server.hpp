/**
 * @file user_server.hpp
 * @brief 用户服务实现类声明（实现见 services/user/src/user_server.cpp）。
 */

#pragma once

#include <memory>
#include <string>

#include <odb/database.hxx>

#include "channel.hpp"
#include "db/user_dao.hpp"
#include "redis.hpp"
#include "user.pb.h"

namespace im {

/**
 * @brief 用户服务实现：注册、登录、登出、用户信息读写。
 */
class UserServiceImpl final : public UserService {
public:
    /**
     * @brief 构造。
     * @param db          MySQL/ODB 数据库对象
     * @param session     登录会话存储
     * @param online      在线状态存储
     * @param channels    服务信道管理器（用于调用 file 服务）
     * @param file_service file 服务名，如 /im/service/file
     */
    UserServiceImpl(std::shared_ptr<odb::core::database> db,
                    std::shared_ptr<SessionStore> session,
                    std::shared_ptr<OnlineStore> online,
                    ServiceManager::Ptr channels,
                    std::string file_service);

    void UserRegister(google::protobuf::RpcController* controller,
                      const UserRegisterReq* request,
                      UserRegisterRsp* response,
                      google::protobuf::Closure* done) override;

    void UserLogin(google::protobuf::RpcController* controller,
                   const UserLoginReq* request,
                   UserLoginRsp* response,
                   google::protobuf::Closure* done) override;

    void UserLogout(google::protobuf::RpcController* controller,
                    const UserLogoutReq* request,
                    UserLogoutRsp* response,
                    google::protobuf::Closure* done) override;

    void GetUserInfo(google::protobuf::RpcController* controller,
                     const GetUserInfoReq* request,
                     GetUserInfoRsp* response,
                     google::protobuf::Closure* done) override;

    void GetMultiUserInfo(google::protobuf::RpcController* controller,
                          const GetMultiUserInfoReq* request,
                          GetMultiUserInfoRsp* response,
                          google::protobuf::Closure* done) override;

    void SetUserNickname(google::protobuf::RpcController* controller,
                         const SetUserNicknameReq* request,
                         SetUserNicknameRsp* response,
                         google::protobuf::Closure* done) override;

    void SetUserDescription(google::protobuf::RpcController* controller,
                            const SetUserDescriptionReq* request,
                            SetUserDescriptionRsp* response,
                            google::protobuf::Closure* done) override;

    void SetUserAvatar(google::protobuf::RpcController* controller,
                       const SetUserAvatarReq* request,
                       SetUserAvatarRsp* response,
                       google::protobuf::Closure* done) override;

private:
    bool fetchAvatar(const std::string& request_id,
                     const std::string& avatar_id,
                     std::string& content);

    bool uploadAvatar(const std::string& request_id,
                      const std::string& content,
                      std::string& file_id);

    static bool validAccount(const std::string& account);
    static bool validNickname(const std::string& nickname);
    static bool validPassword(const std::string& password);

    std::shared_ptr<odb::core::database> db_;
    std::shared_ptr<UserDao> users_;
    std::shared_ptr<SessionStore> session_;
    std::shared_ptr<OnlineStore> online_;
    ServiceManager::Ptr channels_;
    std::string file_service_;
};

} // namespace im
