# 好友服务构建配置
add_executable(friend_server
    src/friend_server.cpp
    include/friend_server.hpp
)
target_include_directories(friend_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(friend_server PRIVATE im_proto im_db im_common)
