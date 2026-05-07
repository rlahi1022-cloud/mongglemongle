#include "monggle/app_config.h"
#include "monggle/auth/auth_service.h"
#include "monggle/event/event_bus.h"
#include "monggle/entry/entry_service.h"
#include "monggle/router/routes.h"

#include <drogon/drogon.h>

#include <memory>

int main() {
    monggle::AppConfig::init();

    auto authService = std::make_shared<monggle::AuthService>();
    auto eventBus = std::make_shared<monggle::EventBus>();
    auto entryService = std::make_shared<monggle::EntryService>(*authService, *eventBus);

    monggle::configureRoutes(entryService);

    LOG_INFO << "monggle server starting on 0.0.0.0:8080";

    drogon::app()
        .addListener("0.0.0.0", 8080)
        .setThreadNum(0)            // 0 = hardware_concurrency
        .setLogLevel(trantor::Logger::kInfo)
        .run();

    return 0;
}
