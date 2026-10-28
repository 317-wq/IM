/**
 * @file crypto.cpp
 * @brief 密码哈希与随机盐工具实现，基于 OpenSSL。
 */

#include "crypto.hpp"

#include <openssl/rand.h>
#include <openssl/sha.h>

#include <stdexcept>
#include <vector>

namespace im {

namespace {

/**
 * @brief 把字节串转成小写 hex（仅本文件使用）。
 */
std::string toHex(const unsigned char* data, std::size_t size) {
    static const char* kDigits = "0123456789abcdef";
    std::string out;
    out.resize(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out[i * 2] = kDigits[(data[i] >> 4) & 0x0f];
        out[i * 2 + 1] = kDigits[data[i] & 0x0f];
    }
    return out;
}

} // namespace

/**
 * @brief 实现 SHA-256：输入任意长度，输出 64 位 hex。
 */
std::string sha256Hex(const std::string& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);
    return toHex(digest, SHA256_DIGEST_LENGTH);
}

/**
 * @brief 实现随机盐：使用 OpenSSL RAND_bytes，输出 hex。
 */
std::string randomHex(std::size_t bytes) {
    std::vector<unsigned char> buffer(bytes);
    if (RAND_bytes(buffer.data(), static_cast<int>(bytes)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    return toHex(buffer.data(), bytes);
}

} // namespace im
