#include "monggle/auth_service.h"

namespace monggle {

bool AuthService::authenticate(const std::string& token) {
    // TODO: verify token and session state with auth backend
    return !token.empty();
}

} // namespace monggle
