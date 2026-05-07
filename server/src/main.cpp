#include "monggle/app_config.h"
#include "monggle/auth/auth_service.h"
#include "monggle/event/event_bus.h"
#include "monggle/entry/entry_service.h"
#include "monggle/router/router.h"
#include "monggle/router/routes.h"

#include <iostream>

int main() {
    monggle::AppConfig::init();

    monggle::AuthService authService;
    monggle::EventBus eventBus;
    monggle::EntryService entryService(authService, eventBus);

    monggle::Router router;
    monggle::configureRoutes(router, entryService);

    std::cout << "몽글몽글 서버 뼈대 시작" << std::endl;
    router.dispatch("/api/entry/create", "dummy-token", "오늘 Redis Pub/Sub로 알림 시스템을 만들었음.");

    return 0;
}
