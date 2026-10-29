# IM 微服务

C++ 微服务 IM 服务端：用户、好友/会话、消息转发、消息存储、文件、网关。
详细开发过程见 `docs/开发计划.md`，接口约定见 `docs/接口设计.md`。

## 依赖

- Ubuntu 24.04、g++ 13、CMake 3.28
- brpc / protobuf / etcd-cpp-apiv3 / ODB(MySQL) / redis++ / AMQP-CPP / Elasticsearch client(cpr) / websocketpp / Boost

中间件：MySQL、Redis、RabbitMQ、etcd、Elasticsearch。

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
```

产物在 `build/bin/`。

## 运行

MySQL 密码通过环境变量传入，不落盘：

```bash
export IM_MYSQL_PASSWORD='你的密码'
./build/bin/user_server     -flagfile=conf/user.conf
./build/bin/friend_server   -flagfile=conf/friend.conf
./build/bin/transmit_server -flagfile=conf/transmit.conf
./build/bin/message_server  -flagfile=conf/message.conf
./build/bin/file_server     -flagfile=conf/file.conf
./build/bin/gateway_server  -flagfile=conf/gateway.conf
```

端口：

| 服务 | 端口 |
| --- | --- |
| gateway HTTP / WS | 9000 / 9001 |
| file | 10001 |
| user | 10002 |
| friend | 10003 |
| transmit | 10004 |
| message | 10005 |

## 数据库

```bash
mysql -uroot -p < sql/user.sql
mysql -uroot -p < sql/friend.sql
mysql -uroot -p < sql/message.sql
```

## 端到端测试

```bash
export IM_MYSQL_PASSWORD='你的密码'
bash scripts/e2e.sh
```

脚本会启动全部服务、注册两个用户、加好友、建单聊、发消息、拉历史，最后清理进程。

## 已知限制

- 还没做 Docker 化部署（当前为单机原生进程）；
- 消息消费是 ack-after-callback，失败可能丢消息；
- 文件接口在网关侧未做鉴权；
- 历史消息未分页。
