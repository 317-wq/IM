# 验收用测试程序构建配置
add_executable(echo_client echo_client.cpp)
target_link_libraries(echo_client PRIVATE im_proto im_common)

add_executable(file_client file_client.cpp)
target_link_libraries(file_client PRIVATE im_proto im_common)

add_executable(daily_file_sink_test daily_file_sink_test.cpp)
target_link_libraries(daily_file_sink_test PRIVATE im_common)

add_executable(db_user_test db_user_test.cpp)
target_link_libraries(db_user_test PRIVATE im_db)

add_executable(user_client user_client.cpp)
target_link_libraries(user_client PRIVATE im_proto im_common)

add_executable(friend_db_test friend_db_test.cpp)
target_link_libraries(friend_db_test PRIVATE im_db)

add_executable(friend_client friend_client.cpp)
target_link_libraries(friend_client PRIVATE im_proto im_common)

# M5 消息转发验收客户端
add_executable(transmit_client transmit_client.cpp)
target_link_libraries(transmit_client PRIVATE im_proto im_common)

# M6 消息查询验收客户端
add_executable(message_client message_client.cpp)
target_link_libraries(message_client PRIVATE im_proto im_common)
