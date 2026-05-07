#pragma once

#include <string>

namespace monggle {

// libxcrypt(crypt_r)를 이용한 bcrypt($2b$) 해싱/검증.
class PasswordService {
public:
    // cost 12 = 약 250ms. 운영에서 13~14 권장.
    static std::string hash(const std::string& password, int cost = 12);

    // 일정 시간 비교가 필요하지만 crypt_r 자체가 상수 시간은 아님.
    // MVP 수준에서는 충분. 운영 시 timing 보호 추가 검토.
    static bool verify(const std::string& password, const std::string& hashedPassword);
};

} // namespace monggle
