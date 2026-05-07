#pragma once

#include <memory>

namespace monggle {

class AuthService;

void configureHealthRoutes();
void configureAuthRoutes(std::shared_ptr<AuthService> authService);

} // namespace monggle
