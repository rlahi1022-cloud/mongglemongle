#pragma once

#include <string>

namespace monggle {

class AuthService {
public:
    bool authenticate(const std::string& token);
};

} // namespace monggle
