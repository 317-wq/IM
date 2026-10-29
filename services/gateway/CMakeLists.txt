# 网关构建配置（HTTP 第一步）
add_executable(gateway_server
    src/gateway_server.cpp
    include/gateway_server.hpp
)
target_include_directories(gateway_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/cpp-httplib
)
target_link_libraries(gateway_server PRIVATE im_proto im_common Boost::system)
