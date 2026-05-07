#include "monggle/app_config.h"

#include <trantor/utils/Logger.h>

namespace monggle {

void AppConfig::init() {
    LOG_INFO << "[AppConfig] init";
    // TODO: load configuration file, environment variables, and secure secrets
}

std::string AppConfig::configPath() {
    return "config/app.yaml";
}

} // namespace monggle
