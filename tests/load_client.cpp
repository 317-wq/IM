/**
 * @file load_client.cpp
 * @brief 容量/压测客户端：支持 prepare（造账号）、login、send 三种场景。
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <gflags/gflags.h>
#include <httplib.h>

#include "logger.hpp"
#include "transmit.pb.h"
#include "user.pb.h"
#include "utils.hpp"

DEFINE_string(op, "login", "场景：prepare/login/send");
DEFINE_string(host, "127.0.0.1", "网关地址");
DEFINE_int32(port, 9000, "网关端口");
DEFINE_int32(count, 20, "总请求数");
DEFINE_int32(concurrency, 4, "并发线程数");
DEFINE_int32(duration, 0, "持续压测秒数，0 表示按 count 固定请求数");
DEFINE_int32(pool, 500, "login 场景可复用的账号数量");
DEFINE_string(prefix, "load", "压测账号前缀");
DEFINE_string(password, "123456", "压测账号密码");
DEFINE_string(account, "carol", "send 场景使用的登录账号");
DEFINE_string(chat_session_id, "", "send 场景的会话 ID");
DEFINE_string(text, "load-test", "消息内容");
DEFINE_bool(run_mode, false, "运行模式");
DEFINE_string(log_dir, "logs/load_client", "发布模式日志目录");
DEFINE_int32(log_level, 0, "日志等级");

namespace {

struct Result {
    int ok = 0;
    int fail = 0;
    std::vector<double> latency_ms;
};

/**
 * @brief 统一的 protobuf POST 请求，返回状态码与耗时。
 */
template <typename ReqT, typename RspT>
bool post(httplib::Client& client, const std::string& path, ReqT& request, RspT& response,
          double& latency_ms) {
    request.set_request_id(im::uuid());
    auto begin = std::chrono::steady_clock::now();
    auto result = client.Post(path.c_str(), request.SerializeAsString(),
                              "application/x-protobuf");
    auto end = std::chrono::steady_clock::now();
    latency_ms = std::chrono::duration<double, std::milli>(end - begin).count();
    return result && result->status == 200 && response.ParseFromString(result->body) &&
           response.success();
}

/**
 * @brief 生成第 i 个压测账号。
 */
std::string accountAt(int i) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s%04d", FLAGS_prefix.c_str(), i);
    return buf;
}

/**
 * @brief 计算分位数。
 */
double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0;
    size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
    return sorted[idx];
}

/**
 * @brief 多线程执行任务并汇总。
 */
template <typename Fn>
Result runWorkers(Fn fn) {
    Result total;
    std::vector<Result> parts(FLAGS_concurrency);
    std::vector<std::thread> threads;

    int base = FLAGS_count / FLAGS_concurrency;
    int extra = FLAGS_count % FLAGS_concurrency;
    int offset = 0;
    for (int t = 0; t < FLAGS_concurrency; ++t) {
        int n = base + (t < extra ? 1 : 0);
        int start = offset;
        offset += n;
        threads.emplace_back([&, t, start, n]() {
            httplib::Client client(FLAGS_host, FLAGS_port);
            client.set_connection_timeout(3, 0);
            client.set_read_timeout(10, 0);
            auto one = [&](int index) {
                double latency = 0;
                if (fn(client, index, latency)) {
                    parts[t].ok++;
                } else {
                    parts[t].fail++;
                }
                parts[t].latency_ms.push_back(latency);
            };
            if (FLAGS_duration > 0) {
                auto deadline = std::chrono::steady_clock::now() +
                                std::chrono::seconds(FLAGS_duration);
                int index = start;
                while (std::chrono::steady_clock::now() < deadline) {
                    one(index++);
                }
            } else {
                for (int i = 0; i < n; ++i) {
                    one(start + i);
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    for (auto& p : parts) {
        total.ok += p.ok;
        total.fail += p.fail;
        total.latency_ms.insert(total.latency_ms.end(), p.latency_ms.begin(),
                                p.latency_ms.end());
    }
    return total;
}

/**
 * @brief 输出统计结果。
 */
void report(const Result& r, double seconds) {
    auto lat = r.latency_ms;
    std::sort(lat.begin(), lat.end());
    const double total = r.ok + r.fail;
    std::printf("total=%d success=%d fail=%d qps=%.1f p50=%.2fms p95=%.2fms p99=%.2fms\n",
                static_cast<int>(total), r.ok, r.fail, total / seconds,
                percentile(lat, 0.50), percentile(lat, 0.95), percentile(lat, 0.99));
}

} // namespace

/**
 * @brief 程序入口。
 */
int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    im::init_logger(FLAGS_run_mode, FLAGS_log_dir, FLAGS_log_level);

    if (FLAGS_op == "prepare") {
        httplib::Client client(FLAGS_host, FLAGS_port);
        int ok = 0;
        for (int i = 0; i < FLAGS_count; ++i) {
            im::UserRegisterReq request;
            request.set_account(accountAt(i));
            request.set_nickname(accountAt(i));
            request.set_password(FLAGS_password);
            im::UserRegisterRsp response;
            double latency = 0;
            if (post(client, "/api/user/register", request, response, latency)) ok++;
        }
        std::printf("prepared=%d/%d\n", ok, FLAGS_count);
        return 0;
    }

    auto begin = std::chrono::steady_clock::now();

    if (FLAGS_op == "login") {
        auto result = runWorkers([](httplib::Client& client, int index, double& latency) {
            im::UserLoginReq request;
            request.set_account(accountAt(index % FLAGS_pool));
            request.set_password(FLAGS_password);
            im::UserLoginRsp response;
            if (!post(client, "/api/user/login", request, response, latency)) {
                return false;
            }
            im::UserLogoutReq logout;
            logout.set_session_id(response.session_id());
            im::UserLogoutRsp logout_rsp;
            double logout_latency = 0;
            post(client, "/api/user/logout", logout, logout_rsp, logout_latency);
            latency += logout_latency;
            return true;
        });
        report(result, std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count());
        return 0;
    }

    if (FLAGS_op == "send") {
        httplib::Client login_client(FLAGS_host, FLAGS_port);
        im::UserLoginReq login_req;
        login_req.set_account(FLAGS_account);
        login_req.set_password(FLAGS_password);
        im::UserLoginRsp login_rsp;
        double latency = 0;
        if (!post(login_client, "/api/user/login", login_req, login_rsp, latency)) {
            std::printf("login %s failed\n", FLAGS_account.c_str());
            return 1;
        }
        const std::string sid = login_rsp.session_id();
        auto result = runWorkers([sid](httplib::Client& client, int, double& latency) {
            im::SendMessageReq request;
            request.set_session_id(sid);
            request.set_chat_session_id(FLAGS_chat_session_id);
            request.mutable_message()->set_message_type(im::MessageType::STRING);
            request.mutable_message()->mutable_string_message()->set_content(FLAGS_text);
            im::SendMessageRsp response;
            return post(client, "/api/message/send", request, response, latency);
        });
        report(result, std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count());
        return 0;
    }

    std::printf("unknown op %s\n", FLAGS_op.c_str());
    return 1;
}
