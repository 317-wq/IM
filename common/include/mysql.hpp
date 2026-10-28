/**
 * @file mysql.hpp
 * @brief ODB 数据库连接工厂声明（实现见 common/src/mysql.cpp）。
 */

#pragma once

#include <memory>
#include <string>

#include <odb/database.hxx>

namespace im {

/**
 * @brief 创建带连接池的 ODB 数据库对象。
 *
 * 连接池由 ODB 的 connection_pool_factory 管理，调用方只持有 database 指针，
 * 每次事务自动取还连接。
 */
class ODBFactory {
public:
    /**
     * @brief 创建数据库对象。
     * @param user      用户名
     * @param password  密码
     * @param host      主机地址
     * @param db        数据库名
     * @param charset   字符集，如 utf8mb4
     * @param port      端口，0 表示默认 3306
     * @param pool_size 连接池大小
     * @return ODB database 对象
     */
    static std::shared_ptr<odb::core::database> create(const std::string& user,
                                                       const std::string& password,
                                                       const std::string& host,
                                                       const std::string& db,
                                                       const std::string& charset,
                                                       unsigned int port,
                                                       int pool_size);
};

} // namespace im
