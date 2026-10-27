/**
 * @file logger.hpp
 * @brief 全局日志封装。
 *
 * 统一本项目的日志输出格式为：
 *   [级别][YYYY-MM-DD HH:MM:SS][源文件绝对路径:行号] 消息
 *
 * 其他模块只通过 IM_TRACE/IM_DEBUG/IM_INFO/IM_WARN/IM_ERROR/IM_FATAL 宏打日志，
 * 这样既能保证格式一致，又能自动带上调用点的文件与行号（依赖 spdlog 的 source_loc）。
 */

#pragma once

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <ctime>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/fmt/fmt.h>

namespace im {

/**
 * @brief 获取全局日志器。
 *
 * 进程启动时由 init_logger() 初始化一次，之后各模块只读使用。
 * 返回的是引用，未初始化时为空 shared_ptr，调用前必须保证 init_logger 已执行。
 */
inline std::shared_ptr<spdlog::logger>& logger() {
    static std::shared_ptr<spdlog::logger> instance;
    return instance;
}

namespace detail {

/**
 * @brief 把 spdlog 的日志级别转换成大写字符串（TRACE/DEBUG/INFO/WARN/ERROR/FATAL）。
 * @param level spdlog 级别枚举
 * @return 级别名称；未知级别返回 "OFF"
 */
inline const char* level_name(spdlog::level::level_enum level) {
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

/**
 * @brief 自定义 spdlog 格式化器，输出一条完整日志：
 *        [级别][本地时间][文件:行号] 消息
 *
 * 不用 set_pattern 的原因：spdlog 的 %l 是小写级别，且没有直接输出大写级别的占位符；
 * 这里手写格式化，时间用 localtime_r + strftime，保证与项目约定格式完全一致。
 */
class LineFormatter final : public spdlog::formatter {
public:
    /**
     * @brief 把一条日志消息格式化后写入 spdlog 的目标缓冲区（末尾自动换行）。
     * @param msg  spdlog 的原始日志消息（含级别、时间、source_loc、正文）
     * @param dest 输出缓冲区
     */
    void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override {
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
     * @brief spdlog 要求 formatter 可复制；当前格式化器无状态，直接拷贝即可。
     */
    std::unique_ptr<spdlog::formatter> clone() const override {
        return std::make_unique<LineFormatter>(*this);
    }
};

} // namespace detail

/**
 * @brief 初始化全局日志器。
 *
 * @param release_mode false：调试模式，输出到控制台（彩色），级别 trace
 *                     true ：发布模式，输出到文件，级别由 level 指定
 * @param file  发布模式下的日志文件路径（调试模式忽略）
 * @param level 发布模式日志级别：0 trace / 1 debug / 2 info / 3 warn / 4 error / 5 critical
 *
 * 注意：必须在创建任何线程之前调用，保证 logger() 初始化完成。
 */
inline void init_logger(bool release_mode, const std::string& file, int level) {
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

// 统一日志宏：自动携带 __FILE__ / __LINE__，格式由 LineFormatter 统一控制
#define IM_TRACE(...) SPDLOG_LOGGER_TRACE(::im::logger(), __VA_ARGS__)
#define IM_DEBUG(...) SPDLOG_LOGGER_DEBUG(::im::logger(), __VA_ARGS__)
#define IM_INFO(...)  SPDLOG_LOGGER_INFO(::im::logger(), __VA_ARGS__)
#define IM_WARN(...)  SPDLOG_LOGGER_WARN(::im::logger(), __VA_ARGS__)
#define IM_ERROR(...) SPDLOG_LOGGER_ERROR(::im::logger(), __VA_ARGS__)
#define IM_FATAL(...) SPDLOG_LOGGER_CRITICAL(::im::logger(), __VA_ARGS__)
