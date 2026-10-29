#!/usr/bin/env bash
# M8 端到端回归：起全部服务 -> 注册/登录 -> 加好友 -> 建会话 -> 发消息 -> 拉历史 -> 清理。
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ -z "${IM_MYSQL_PASSWORD:-}" ]]; then
    echo "请先 export IM_MYSQL_PASSWORD=<mysql root 密码>"
    exit 1
fi

mkdir -p logs
pids=()
cleanup() {
    for pid in "${pids[@]:-}"; do
        kill "$pid" 2>/dev/null || true
    done
}
trap cleanup EXIT

start() {
    local name=$1
    shift
    "$@" >"logs/e2e-${name}.log" 2>&1 &
    pids+=("$!")
}

wait_port() {
    local port=$1
    for _ in $(seq 1 80); do
        if (exec 3<>"/dev/tcp/127.0.0.1/${port}") 2>/dev/null; then
            exec 3>&-
            return 0
        fi
        sleep 0.1
    done
    echo "端口 ${port} 启动超时"
    exit 1
}

start file ./build/bin/file_server -flagfile=conf/file.conf
start user ./build/bin/user_server -flagfile=conf/user.conf
start friend ./build/bin/friend_server -flagfile=conf/friend.conf
start transmit ./build/bin/transmit_server -flagfile=conf/transmit.conf
start message ./build/bin/message_server -flagfile=conf/message.conf
start gateway ./build/bin/gateway_server -flagfile=conf/gateway.conf

for port in 10001 10002 10003 10004 10005 9000; do
    wait_port "$port"
done
echo "[PASS] 全部服务已启动"

suffix=$(date +%s)
account_a="ea${suffix}"
account_b="eb${suffix}"
nick_a="a${suffix}"
nick_b="b${suffix}"

reg_a=$(./build/bin/gateway_client -op=register -account="$account_a" -nickname="$nick_a" -password=123456)
uid_a=$(echo "$reg_a" | sed -n 's/^user_id=//p')
reg_b=$(./build/bin/gateway_client -op=register -account="$account_b" -nickname="$nick_b" -password=123456)
uid_b=$(echo "$reg_b" | sed -n 's/^user_id=//p')
[[ -n "$uid_a" && -n "$uid_b" ]]
echo "[PASS] 注册两个用户"

login_a=$(./build/bin/gateway_client -op=login -account="$account_a" -password=123456)
sid_a=$(echo "$login_a" | sed -n 's/^session_id=//p')
[[ -n "$sid_a" ]]
echo "[PASS] 用户 A 登录"

event=$(./build/bin/friend_client -op=add -user_id="$uid_a" -peer_id="$uid_b" |
    sed -n 's/^event_id=//p')
[[ -n "$event" ]]
./build/bin/friend_client -op=process -user_id="$uid_b" -apply_user_id="$uid_a" \
    -event_id="$event" -agree=true >/dev/null
echo "[PASS] 好友申请与同意"

sessions=$(./build/bin/gateway_client -op=sessions -session_id="$sid_a")
chat_sid=$(echo "$sessions" | grep '^session_id=' | head -1 | sed -n 's/^session_id=\([^ ]*\).*/\1/p')
[[ -n "$chat_sid" ]]
echo "[PASS] 会话已创建: $chat_sid"

./build/bin/gateway_client -op=send -session_id="$sid_a" -chat_session_id="$chat_sid" \
    -text=e2e-hello >/dev/null
echo "[PASS] 网关发送消息"

history=$(./build/bin/gateway_client -op=history -session_id="$sid_a" \
    -chat_session_id="$chat_sid")
count=$(echo "$history" | sed -n 's/^count=//p')
[[ "${count:-0}" -ge 1 ]]
echo "[PASS] 历史消息 count=$count"

echo "ALL PASS"
