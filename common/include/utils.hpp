/**
 * @file utils.hpp
 * @brief 公共工具函数声明：时间戳、唯一 ID、文件读写（实现见 common/src/utils.cpp）。
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

/**
 * @brief 读取整个文件内容（二进制）。
 * @param path 文件路径
 * @param body 输出：文件内容
 * @return 打开或读取失败返回 false
 */
bool readFile(const std::string& path, std::string& body);

/**
 * @brief 把内容整体写入文件（二进制，覆盖写）。
 * @param path 文件路径
 * @param body 要写入的内容
 * @return 打开或写入失败返回 false
 */
bool writeFile(const std::string& path, const std::string& body);

} // namespace im
