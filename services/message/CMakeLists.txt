# 消息存储服务构建配置
add_executable(message_server
    src/message_server.cpp
    include/message_server.hpp
)
target_include_directories(message_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(message_server PRIVATE im_proto im_db im_common)
