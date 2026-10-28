# 文件服务构建配置
# 目录约定：include/ 放声明（.hpp），src/ 放实现（.cpp）
add_executable(file_server
    src/file_server.cpp
    include/file_server.hpp
)
target_include_directories(file_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(file_server PRIVATE im_proto im_common)
