/**
 * @file utils.cpp
 * @brief 公共工具函数实现：时间戳与唯一 ID。
 */

#include "utils.hpp"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

namespace im {

/**
 * @brief 实现毫秒时间戳。
 */
int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

/**
 * @brief 实现秒时间戳。
 */
int64_t now_s() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

/**
 * @brief 实现 UUID：随机数 + 进程内自增序号拼成 8-4-4-4-12。
 */
std::string uuid() {
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
