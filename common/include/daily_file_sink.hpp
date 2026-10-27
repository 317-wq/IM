/**
 * @file daily_file_sink.hpp
 * @brief 按天滚动的文件 sink 声明（实现见 common/src/daily_file_sink.cpp）。
 *
 * 输出路径：<dir>/<YYYY-MM-DD>.log
 * 例如：logs/echo/2026-10-27.log
 * 跨天时自动关闭旧文件并打开新日期的文件，进程无需重启。
 */

#pragma once

#include <fstream>
#include <mutex>
#include <string>

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

namespace im {

/**
 * @brief 按本地日期切分的文件 sink。
 *
 * 线程安全由 base_sink<std::mutex> 保证；每写一条日志前检查日期，
 * 日期变化则切换文件，因此同一天内始终追加写同一个文件。
 */
class DailyFileSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    /**
     * @brief 构造 sink 并确保目录存在。
     * @param dir 日志目录，如 "logs/echo"；不存在会自动创建
     */
    explicit DailyFileSink(std::string dir);

protected:
    /**
     * @brief 写一条日志：必要时切换日期文件，格式化后追加写入。
     * @param msg spdlog 原始日志消息
     */
    void sink_it_(const spdlog::details::log_msg& msg) override;

    /**
     * @brief 刷新文件缓冲。
     */
    void flush_() override;

private:
    std::string dir_;      ///< 日志目录（构造时补上结尾的 '/'）
    std::string date_;     ///< 当前打开文件对应的日期，格式 YYYY-MM-DD
    std::ofstream file_;   ///< 当前日期对应的日志文件
};

} // namespace im
