#ifndef HAIKODE_CORE_CRYPTO_H
#define HAIKODE_CORE_CRYPTO_H

#include <cstdint>
#include <string>
#include <vector>

namespace Crypto {

std::vector<uint8_t> Sha256Bytes(const std::string& input);
std::string Sha256Hex(const std::string& input);
std::string Base64UrlEncode(const std::vector<uint8_t>& bytes);
std::string RandomBase64Url(size_t byteCount);

}

#endif

