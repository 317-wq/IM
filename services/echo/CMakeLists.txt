# echo 示例服务构建配置
# 可执行文件统一输出到 build/bin（由根 CMakeLists 的 CMAKE_RUNTIME_OUTPUT_DIRECTORY 控制）
add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server PRIVATE im_proto im_deps)
