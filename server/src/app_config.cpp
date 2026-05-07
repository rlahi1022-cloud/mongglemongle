#include "monggle/app_config.h"
#include <iostream>

namespace monggle {

void AppConfig::init() {
    std::cout << "[AppConfig] init" << std::endl;
    // TODO: load configuration file, environment variables, and secure secrets
}

std::string AppConfig::configPath() {
    return "config/app.yaml";
}

} // namespace monggle
