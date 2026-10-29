# IM 服务端根构建脚本
#
# 职责：
#   1. 声明 C++17 与输出目录（可执行文件统一放到 build/bin）
#   2. 查找公共依赖（brpc / protobuf / gflags / spdlog / fmt / openssl / zlib / etcd / odb / redis）
#   3. 引入 protoc 与 ODB 生成辅助函数
#   4. 编译公共基础设施库 im_common、实体库 im_odb、DAO 库 im_db
#   5. 按服务注册子目录（后续里程碑继续往下加 add_subdirectory）

cmake_minimum_required(VERSION 3.20)
project(im LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# 依赖查找
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(BRPC      REQUIRED IMPORTED_TARGET brpc)
pkg_check_modules(GFLAGS    REQUIRED IMPORTED_TARGET gflags)
pkg_check_modules(SPDLOG    REQUIRED IMPORTED_TARGET spdlog)
pkg_check_modules(FMT       REQUIRED IMPORTED_TARGET fmt)
pkg_check_modules(ODB_MYSQL REQUIRED IMPORTED_TARGET libodb-mysql)
pkg_check_modules(ODB_BOOST REQUIRED IMPORTED_TARGET libodb-boost)
pkg_check_modules(REDISPP   REQUIRED IMPORTED_TARGET redis++)
pkg_check_modules(AMQPCPP   REQUIRED IMPORTED_TARGET amqpcpp)

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(etcd-cpp-api REQUIRED)
find_package(Boost REQUIRED COMPONENTS date_time)
find_library(CPR_LIBRARY NAMES cpr REQUIRED)
find_library(EV_LIBRARY NAMES ev REQUIRED)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Proto.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Odb.cmake)

# ---------------------------------------------------------------------------
# 公共依赖目标：所有服务/测试链接它即可，避免每个 target 重复写一长串库
# 后续里程碑会往这里补 amqp / elasticlient 等
# ---------------------------------------------------------------------------
add_library(im_deps INTERFACE)
target_include_directories(im_deps INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(im_deps INTERFACE
    PkgConfig::BRPC
    PkgConfig::GFLAGS
    PkgConfig::SPDLOG
    PkgConfig::FMT
    PkgConfig::ODB_MYSQL
    PkgConfig::ODB_BOOST
    PkgConfig::REDISPP
    protobuf::libprotobuf
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    leveldb
    ZLIB::ZLIB
    dl
    etcd-cpp-api
    Boost::date_time
    ${CPR_LIBRARY}
    PkgConfig::AMQPCPP
    ${EV_LIBRARY}
)

# ---------------------------------------------------------------------------
# 公共基础设施库：声明放 common/include，实现放 common/src
# ---------------------------------------------------------------------------
add_library(im_common STATIC
    common/src/logger.cpp
    common/src/daily_file_sink.cpp
    common/src/utils.cpp
    common/src/etcd.cpp
    common/src/channel.cpp
    common/src/mysql.cpp
    common/src/redis.cpp
    common/src/crypto.cpp
    common/src/es.cpp
    common/src/mq.cpp
)
target_include_directories(im_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
target_link_libraries(im_common PUBLIC im_deps)

# ---------------------------------------------------------------------------
# ODB 实体与 DAO
# ---------------------------------------------------------------------------
im_add_odb_lib(im_odb
    ODB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/odb
    FILES
        user.hxx
        relation.hxx
        friend_apply.hxx
        chat_session.hxx
        chat_session_member.hxx
        message.hxx
)

add_library(im_db STATIC
    common/src/db/user_dao.cpp
    common/src/db/relation_dao.cpp
    common/src/db/friend_apply_dao.cpp
    common/src/db/chat_session_dao.cpp
    common/src/db/chat_session_member_dao.cpp
    common/src/db/friend_repository.cpp
    common/src/db/message_dao.cpp
)
target_include_directories(im_db PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
target_link_libraries(im_db PUBLIC im_odb im_common)

# ---------------------------------------------------------------------------
# 接口定义：按里程碑逐个加入
# ---------------------------------------------------------------------------
im_add_proto_lib(im_proto
    PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto
    PROTOS
        base.proto
        echo.proto
        file.proto
        user.proto
        friend.proto
        transmit.proto
        message.proto
)

# ---------------------------------------------------------------------------
# 服务与测试
# ---------------------------------------------------------------------------
add_subdirectory(services/echo)
add_subdirectory(services/file)
add_subdirectory(services/user)
add_subdirectory(services/friend)
add_subdirectory(services/transmit)
add_subdirectory(services/message)
add_subdirectory(services/gateway)
add_subdirectory(tests)
