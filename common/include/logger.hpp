/**
 * @file logger.hpp
 * @brief 全局日志器声明（实现见 common/logger.cpp）。
 *
 * 统一日志格式：
 *   [级别][YYYY-MM-DD HH:MM:SS][源文件绝对路径:行号] 消息
 *
 * 发布模式按天落盘：<log_dir>/<YYYY-MM-DD>.log（由 DailyFileSink 实现）。
 * 其他模块只通过 IM_TRACE/IM_DEBUG/IM_INFO/IM_WARN/IM_ERROR/IM_FATAL 宏打日志。
 */

#pragma once

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <memory>
#include <string>

#include <spdlog/formatter.h>

#include <spdlog/spdlog.h>

namespace im {

/**
 * @brief 获取全局日志器。
 *
 * 进程启动时由 init_logger() 初始化一次，之后各模块只读使用。
 * 返回引用，未初始化时为空 shared_ptr，调用前必须保证 init_logger 已执行。
 */
std::shared_ptr<spdlog::logger>& logger();

namespace detail {

/**
 * @brief 自定义 spdlog 格式化器声明。
 *
 * 输出：[级别][本地时间][文件:行号] 消息。
 * 不用 set_pattern 的原因：spdlog 的 %l 是小写级别，且没有输出大写级别的占位符。
 */
class LineFormatter final : public spdlog::formatter {
public:
    /**
     * @brief 把一条日志消息格式化后写入目标缓冲区（末尾自动换行）。
     * @param msg  spdlog 原始日志消息（含级别、时间、source_loc、正文）
     * @param dest 输出缓冲区
     */
    void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override;

    /**
     * @brief spdlog 要求 formatter 可复制；当前格式化器无状态，拷贝即可。
     */
    std::unique_ptr<spdlog::formatter> clone() const override;
};

} // namespace detail

/**
 * @brief 初始化全局日志器。
 *
 * @param release_mode false：调试模式，输出到控制台（彩色），级别 trace
 *                     true ：发布模式，按天写入 log_dir/<YYYY-MM-DD>.log
 * @param log_dir 发布模式下的日志目录，如 "logs/echo"（调试模式忽略）
 * @param level   发布模式日志级别：0 trace / 1 debug / 2 info / 3 warn / 4 error / 5 critical
 *
 * 注意：必须在创建任何线程之前调用。
 */
void init_logger(bool release_mode, const std::string& log_dir, int level);

} // namespace im

// 统一日志宏：自动携带 __FILE__ / __LINE__，格式由 LineFormatter 统一控制
#define IM_TRACE(...) SPDLOG_LOGGER_TRACE(::im::logger(), __VA_ARGS__)
#define IM_DEBUG(...) SPDLOG_LOGGER_DEBUG(::im::logger(), __VA_ARGS__)
#define IM_INFO(...)  SPDLOG_LOGGER_INFO(::im::logger(), __VA_ARGS__)
#define IM_WARN(...)  SPDLOG_LOGGER_WARN(::im::logger(), __VA_ARGS__)
#define IM_ERROR(...) SPDLOG_LOGGER_ERROR(::im::logger(), __VA_ARGS__)
#define IM_FATAL(...) SPDLOG_LOGGER_CRITICAL(::im::logger(), __VA_ARGS__)
