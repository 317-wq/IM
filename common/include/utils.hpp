/**
 * @file utils.hpp
 * @brief 公共工具函数声明：时间戳与唯一 ID（实现见 common/utils.cpp）。
 */

#pragma once

#include <cstdint>
#include <string>

namespace im {

/**
 * @brief 当前时间戳（毫秒）。
 * @return 自 Unix epoch 起的毫秒数
 */
int64_t now_ms();

/**
 * @brief 当前时间戳（秒）。
 * @return 自 Unix epoch 起的秒数
 */
int64_t now_s();

/**
 * @brief 生成一个 36 字符的 UUID 字符串（8-4-4-4-12）。
 *
 * 组成方式：线程内随机数（mt19937_64）+ 进程内原子自增序号，
 * 保证同一台机器上并发调用也不会重复；不追求跨机器全局唯一。
 *
 * @return 形如 "3f2a1b4c-8d5e-0001-a7c9-1e2f3a4b5c6d" 的字符串
 */
std::string uuid();

} // namespace im
