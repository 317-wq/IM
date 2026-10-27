/**
 * @file utils.hpp
 * @brief 公共工具函数：时间戳与唯一 ID。
 *
 * 这些函数会被多个服务共用，因此只放头文件内联实现，避免额外链接库。
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace im {

/**
 * @brief 当前时间戳（毫秒）。
 * @return 自 Unix epoch 起的毫秒数
 */
inline int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

/**
 * @brief 当前时间戳（秒）。
 * @return 自 Unix epoch 起的秒数
 */
inline int64_t now_s() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

/**
 * @brief 生成一个 36 字符的 UUID 字符串（8-4-4-4-12）。
 *
 * 组成方式：线程内随机数（mt19937_64）+ 进程内原子自增序号，
 * 保证同一台机器上并发调用也不会重复；不追求跨机器全局唯一。
 *
 * @return 形如 "3f2a1b4c-8d5e-0001-a7c9-1e2f3a4b5c6d" 的字符串
 */
inline std::string uuid() {
    static thread_local std::mt19937_64 gen{std::random_device{}()};
    static std::atomic<uint32_t> seq{0};
    std::uniform_int_distribution<uint64_t> dist;

    const uint64_t a = dist(gen);
    const uint64_t b = dist(gen);
    const uint32_t c = seq.fetch_add(1, std::memory_order_relaxed);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << static_cast<uint32_t>(a & 0xffffffffu) << '-';
    oss << std::setw(4) << static_cast<uint16_t>((a >> 32) & 0xffffu) << '-';
    oss << std::setw(4) << static_cast<uint16_t>(c & 0xffffu) << '-';
    oss << std::setw(4) << static_cast<uint16_t>((b >> 48) & 0xffffu) << '-';
    oss << std::setw(12) << (b & 0xffffffffffffull);
    return oss.str();
}

} // namespace im
