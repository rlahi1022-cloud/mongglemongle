#include "monggle/auth/password_service.h"

#include <crypt.h>
#include <openssl/rand.h>

#include <array>
#include <cstring>
#include <stdexcept>

namespace monggle {

std::string PasswordService::hash(const std::string& password, int cost) {
    if (cost < 4 || cost > 31) cost = 12;

    // libxcrypt의 안전한 salt 생성기 사용 (bcrypt $2b$)
    char setting[CRYPT_GENSALT_OUTPUT_SIZE];
    std::array<unsigned char, 16> entropy{};
    if (RAND_bytes(entropy.data(), entropy.size()) != 1) {
        throw std::runtime_error("RAND_bytes failed for bcrypt salt");
    }
    char* gen = crypt_gensalt_rn(
        "$2b$", cost,
        reinterpret_cast<const char*>(entropy.data()), entropy.size(),
        setting, sizeof(setting));
    if (!gen) {
        throw std::runtime_error("crypt_gensalt_rn failed");
    }

    crypt_data data{};
    const char* result = crypt_r(password.c_str(), setting, &data);
    if (!result || result[0] == '*') {
        throw std::runtime_error("crypt_r failed (bcrypt unsupported on this libc)");
    }
    return std::string(result);
}

bool PasswordService::verify(const std::string& password, const std::string& hashedPassword) {
    if (hashedPassword.empty()) return false;

    crypt_data data{};
    const char* result = crypt_r(password.c_str(), hashedPassword.c_str(), &data);
    if (!result || result[0] == '*') return false;

    const std::string actual{result};
    if (actual.size() != hashedPassword.size()) return false;
    unsigned diff = 0;
    for (size_t i = 0; i < actual.size(); ++i) {
        diff |= static_cast<unsigned>(actual[i]) ^ static_cast<unsigned>(hashedPassword[i]);
    }
    return diff == 0;
}

} // namespace monggle
