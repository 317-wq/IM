# IM 服务端根构建脚本
#
# 职责：
#   1. 声明 C++17 与输出目录（可执行文件统一放到 build/bin）
#   2. 查找公共依赖（brpc / protobuf / gflags / spdlog / fmt / openssl / zlib / etcd）
#   3. 引入 protoc 生成辅助函数，并把 proto 编译成 im_proto 静态库
#   4. 编译公共基础设施库 im_common（日志、工具、etcd、信道管理等）
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
pkg_check_modules(BRPC   REQUIRED IMPORTED_TARGET brpc)
pkg_check_modules(GFLAGS REQUIRED IMPORTED_TARGET gflags)
pkg_check_modules(SPDLOG REQUIRED IMPORTED_TARGET spdlog)
pkg_check_modules(FMT    REQUIRED IMPORTED_TARGET fmt)

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(etcd-cpp-api REQUIRED)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Proto.cmake)

# ---------------------------------------------------------------------------
# 公共依赖目标：所有服务/测试链接它即可，避免每个 target 重复写一长串库
# 后续里程碑会往这里补 redis++ / amqp / elasticlient / odb 等
# ---------------------------------------------------------------------------
add_library(im_deps INTERFACE)
target_include_directories(im_deps INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(im_deps INTERFACE
    PkgConfig::BRPC
    PkgConfig::GFLAGS
    PkgConfig::SPDLOG
    PkgConfig::FMT
    protobuf::libprotobuf
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    leveldb
    ZLIB::ZLIB
    dl
    etcd-cpp-api
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
)
target_include_directories(im_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
target_link_libraries(im_common PUBLIC im_deps)

# ---------------------------------------------------------------------------
# 接口定义：按里程碑逐个加入
# ---------------------------------------------------------------------------
im_add_proto_lib(im_proto
    PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto
    PROTOS
        echo.proto
        file.proto
)

# ---------------------------------------------------------------------------
# 服务与测试
# ---------------------------------------------------------------------------
add_subdirectory(services/echo)
add_subdirectory(services/file)
add_subdirectory(tests)
