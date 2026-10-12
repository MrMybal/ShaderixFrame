#pragma once
#include "core/Common.h"
#include <span>
#include <stdexcept>
#include <windows.h>

#include <wincrypt.h>
namespace sf {
inline std::string encodeBase64(std::span<const unsigned char> bytes) {
    if (bytes.empty())
        return {};
    DWORD size = 0;
    if (!CryptBinaryToStringA(bytes.data(), DWORD(bytes.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              nullptr, &size))
        throw std::runtime_error("Encodage d'état plugin échoué");
    std::string result(size, '\0');
    if (!CryptBinaryToStringA(bytes.data(), DWORD(bytes.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              result.data(), &size))
        throw std::runtime_error("Encodage d'état plugin échoué");
    result.resize(size);
    if (!result.empty() && result.back() == '\0')
        result.pop_back();
    return result;
}
inline std::vector<unsigned char> decodeBase64(const std::string &text, size_t maximum = 4 * 1024 * 1024) {
    if (text.empty())
        return {};
    if (text.size() > 4 * ((maximum + 2) / 3) || text.size() % 4)
        throw std::runtime_error("État plugin base64 invalide ou trop grand");
    for (char c : text)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' ||
              c == '/' || c == '='))
            throw std::runtime_error("État plugin base64 invalide");
    DWORD count = 0;
    if (!CryptStringToBinaryA(text.data(), DWORD(text.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_STRICT,
                              nullptr, &count, nullptr, nullptr) ||
        count > maximum)
        throw std::runtime_error("État plugin base64 invalide");
    std::vector<unsigned char> bytes(count);
    if (!CryptStringToBinaryA(text.data(), DWORD(text.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_STRICT,
                              bytes.data(), &count, nullptr, nullptr))
        throw std::runtime_error("Décodage d'état plugin échoué");
    return bytes;
}
} // namespace sf
