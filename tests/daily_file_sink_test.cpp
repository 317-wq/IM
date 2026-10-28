/**
 * @file daily_file_sink_test.cpp
 * @brief 跨天日志切换测试。
 *
 * 不修改系统时间，而是直接构造不同日期的 spdlog::log_msg 注入 sink，
 * 验证：
 *   1. 同一天的多条日志写入同一个 <date>.log；
 *   2. 跨天后自动切换到新日期文件；
 *   3. 两个文件的内容互不串扰。
 */

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <spdlog/details/log_msg.h>

#include "daily_file_sink.hpp"
#include "logger.hpp"
#include "utils.hpp"

namespace {

/**
 * @brief 构造本地时区的指定时间点。
 */
std::chrono::system_clock::time_point makeTime(int year, int month, int day,
                                               int hour, int minute, int second) {
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

/**
 * @brief 向 sink 注入一条指定时间、指定正文的日志。
 */
void emit(im::DailyFileSink& sink,
          const std::chrono::system_clock::time_point& time,
          const char* text) {
    spdlog::details::log_msg msg(
        time,
        spdlog::source_loc{"daily_file_sink_test.cpp", 66, "emit"},
        spdlog::string_view_t("test", 4),
        spdlog::level::info,
        spdlog::string_view_t(text, std::strlen(text)));
    sink.log(msg);
}

/**
 * @brief 读取文件内容；文件不存在返回空串。
 */
std::string readAll(const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(ifs),
                       std::istreambuf_iterator<char>());
}

} // namespace

/**
 * @brief 测试入口：写两天日志 -> 校验文件与内容 -> 清理临时目录。
 */
int main() {
    const auto dir = std::filesystem::temp_directory_path() / ("im_log_test_" + im::uuid());
    std::filesystem::remove_all(dir);

    {
        im::DailyFileSink sink(dir.string());
        sink.set_formatter(std::make_unique<im::detail::LineFormatter>());

        emit(sink, makeTime(2026, 10, 27, 23, 59, 59), "day1-a");
        emit(sink, makeTime(2026, 10, 27, 23, 59, 59), "day1-b");
        emit(sink, makeTime(2026, 10, 28, 0, 0, 1), "day2-a");
        sink.flush();
    }

    const auto day1 = dir / "2026-10-27.log";
    const auto day2 = dir / "2026-10-28.log";

    const std::string day1_content = readAll(day1);
    const std::string day2_content = readAll(day2);

    int failures = 0;
    auto check = [&failures](bool ok, const char* what) {
        std::printf("%s %s\n", ok ? "[PASS]" : "[FAIL]", what);
        if (!ok) {
            ++failures;
        }
    };

    check(std::filesystem::exists(day1), "2026-10-27.log 存在");
    check(std::filesystem::exists(day2), "2026-10-28.log 存在");
    check(day1_content.find("day1-a") != std::string::npos, "27 日包含 day1-a");
    check(day1_content.find("day1-b") != std::string::npos, "27 日包含 day1-b");
    check(day1_content.find("day2-a") == std::string::npos, "27 日不包含 day2-a");
    check(day2_content.find("day2-a") != std::string::npos, "28 日包含 day2-a");
    check(day2_content.find("day1-a") == std::string::npos, "28 日不包含 day1-a");

    std::filesystem::remove_all(dir);
    std::printf("%s\n", failures == 0 ? "ALL PASS" : "SOME FAILED");
    return failures == 0 ? 0 : 1;
}
