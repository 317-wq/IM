# 用户服务构建配置
# 目录约定：include/ 放声明（.hpp），src/ 放实现（.cpp）
add_executable(user_server
    src/user_server.cpp
    include/user_server.hpp
)
target_include_directories(user_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(user_server PRIVATE im_proto im_db im_common)
