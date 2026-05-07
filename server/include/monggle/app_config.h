#pragma once

#include <string>

namespace monggle {

struct AppConfig {
    static void init();
    static std::string configPath();
};

} // namespace monggle
