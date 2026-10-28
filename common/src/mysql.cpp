/**
 * @file mysql.cpp
 * @brief ODB 数据库连接工厂实现。
 */

#include "mysql.hpp"

#include <memory>

#include <odb/mysql/connection-factory.hxx>
#include <odb/mysql/database.hxx>

namespace im {

/**
 * @brief 实现创建：先建连接池工厂，再交给 mysql::database 管理。
 */
std::shared_ptr<odb::core::database> ODBFactory::create(const std::string& user,
                                                        const std::string& password,
                                                        const std::string& host,
                                                        const std::string& db,
                                                        const std::string& charset,
                                                        unsigned int port,
                                                        int pool_size) {
    auto pool = std::make_unique<odb::mysql::connection_pool_factory>(
        static_cast<size_t>(pool_size), 0);

    return std::make_shared<odb::mysql::database>(
        user, password, db, host, port, "", charset, 0, std::move(pool));
}

} // namespace im
