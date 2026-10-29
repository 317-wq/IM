# 消息转发服务构建配置
add_executable(transmit_server
    src/transmit_server.cpp
    include/transmit_server.hpp
)
target_include_directories(transmit_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(transmit_server PRIVATE im_proto im_db im_common)
