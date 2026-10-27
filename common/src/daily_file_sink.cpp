/**
 * @file daily_file_sink.cpp
 * @brief 按天滚动的文件 sink 实现。
 */

#include "daily_file_sink.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>

namespace im {

/**
 * @brief 实现构造：创建日志目录，并统一补上结尾的 '/' 方便拼文件名。
 */
DailyFileSink::DailyFileSink(std::string dir) : dir_(std::move(dir)) {
    std::filesystem::create_directories(dir_);
    if (dir_.back() != '/') {
        dir_.push_back('/');
    }
}

/**
 * @brief 实现写入：按消息时间算出日期，跨天则切换文件，然后格式化追加。
 */
void DailyFileSink::sink_it_(const spdlog::details::log_msg& msg) {
    std::time_t t = std::chrono::system_clock::to_time_t(msg.time);
    std::tm tm{};
    localtime_r(&t, &tm);

    char date[11];
    std::strftime(date, sizeof(date), "%Y-%m-%d", &tm);

    if (date_ != date || !file_.is_open()) {
        if (file_.is_open()) {
            file_.close();
        }
        date_ = date;
        file_.open(dir_ + date_ + ".log", std::ios::app);
        if (!file_.is_open()) {
            return;
        }
    }

    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);
    file_.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
}

/**
 * @brief 实现 flush：把文件缓冲刷到磁盘。
 */
void DailyFileSink::flush_() {
    if (file_.is_open()) {
        file_.flush();
    }
}

} // namespace im
