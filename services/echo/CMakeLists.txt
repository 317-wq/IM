# echo 示例服务构建配置
# 目录约定：include/ 放声明（.hpp），src/ 放实现（.cpp）
# 可执行文件统一输出到 build/bin（由根 CMakeLists 的 CMAKE_RUNTIME_OUTPUT_DIRECTORY 控制）
add_executable(echo_server
    src/echo_server.cpp
    include/echo_server.hpp
)
target_include_directories(echo_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(echo_server PRIVATE im_proto im_common)
