# 验收用测试程序构建配置
# 每个里程碑的测试客户端都放在本目录，统一链接接口库与公共基础设施库
add_executable(echo_client echo_client.cpp)
target_link_libraries(echo_client PRIVATE im_proto im_common)

add_executable(file_client file_client.cpp)
target_link_libraries(file_client PRIVATE im_proto im_common)

# 跨天日志切换测试：直接注入不同日期的日志消息，不依赖系统时间
add_executable(daily_file_sink_test daily_file_sink_test.cpp)
target_link_libraries(daily_file_sink_test PRIVATE im_common)

# M2 数据层测试：user 实体的插入、查询、更新与唯一约束
add_executable(db_user_test db_user_test.cpp)
target_link_libraries(db_user_test PRIVATE im_db)
