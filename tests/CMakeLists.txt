# 验收用测试程序构建配置
# 每个里程碑的测试客户端都放在本目录，统一链接 im_proto 与公共依赖
add_executable(echo_client echo_client.cpp)
target_link_libraries(echo_client PRIVATE im_proto im_deps)
