/**
 * @file logger.cpp
 * @brief 全局日志器实现：格式化器、日志器单例与初始化。
 */

#include "logger.hpp"

#include <ctime>
#include <iterator>
#include <string_view>

#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace im {

namespace {

/**
 * @brief 把 spdlog 级别转换成大写字符串（仅本文件使用）。
 * @param level spdlog 级别枚举
 * @return TRACE/DEBUG/INFO/WARN/ERROR/FATAL；未知级别返回 "OFF"
 */
const char* level_name(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::trace:    return "TRACE";
        case spdlog::level::debug:    return "DEBUG";
        case spdlog::level::info:     return "INFO";
        case spdlog::level::warn:     return "WARN";
        case spdlog::level::err:      return "ERROR";
        case spdlog::level::critical: return "FATAL";
        default:                      return "OFF";
    }
}

} // namespace

/**
 * @brief 全局日志器单例：首次调用时构造，之后返回同一引用。
 */
std::shared_ptr<spdlog::logger>& logger() {
    static std::shared_ptr<spdlog::logger> instance;
    return instance;
}

namespace detail {

/**
 * @brief 实现格式化：拼出 [级别][时间][文件:行] 正文，并在末尾补换行。
 */
void LineFormatter::format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) {
    std::time_t t = std::chrono::system_clock::to_time_t(msg.time);
    std::tm tm{};
    localtime_r(&t, &tm);

    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    const char* file = msg.source.filename ? msg.source.filename : "unknown";

    fmt::format_to(std::back_inserter(dest), "[{}][{}][{}:{}] {}\n",
                   level_name(msg.level), ts, file, msg.source.line,
                   std::string_view(msg.payload.data(), msg.payload.size()));
}

/**
 * @brief 实现 clone：spdlog 复制 logger 时会调用，当前无状态直接拷贝。
 */
std::unique_ptr<spdlog::formatter> LineFormatter::clone() const {
    return std::make_unique<LineFormatter>(*this);
}

} // namespace detail

/**
 * @brief 实现初始化：按模式创建控制台或文件日志器，并安装统一格式化器。
 */
void init_logger(bool release_mode, const std::string& file, int level) {
    std::shared_ptr<spdlog::logger> l;
    if (!release_mode) {
        l = spdlog::stdout_color_mt("im");
        l->set_level(spdlog::level::trace);
        l->flush_on(spdlog::level::trace);
    } else {
        l = spdlog::basic_logger_mt("im", file);
        auto lv = static_cast<spdlog::level::level_enum>(level);
        l->set_level(lv);
        l->flush_on(lv);
    }

    l->set_formatter(std::make_unique<detail::LineFormatter>());
    logger() = l;
}

} // namespace im
