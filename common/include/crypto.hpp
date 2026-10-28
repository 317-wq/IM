/**
 * @file crypto.hpp
 * @brief 密码哈希与随机盐工具声明（实现见 common/src/crypto.cpp）。
 */

#pragma once

#include <cstddef>
#include <string>

namespace im {

/**
 * @brief 计算 SHA-256 并转成小写十六进制字符串。
 * @param data 原始数据
 * @return 64 字符 hex
 */
std::string sha256Hex(const std::string& data);

/**
 * @brief 生成指定字节数的随机数据，返回十六进制字符串。
 * @param bytes 随机字节数
 * @return 长度为 bytes * 2 的 hex 字符串
 */
std::string randomHex(std::size_t bytes);

} // namespace im
