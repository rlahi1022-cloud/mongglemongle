#include "monggle/entry_service.h"
#include "monggle/auth_service.h"
#include "monggle/event_bus.h"
#include <iostream>

namespace monggle {

EntryService::EntryService(AuthService& auth, EventBus& bus)
    : authService(auth), eventBus(bus) {
}

void EntryService::createEntry(const std::string& userToken, const std::string& content) {
    if (!authService.authenticate(userToken)) {
        std::cerr << "[EntryService] unauthorized" << std::endl;
        return;
    }
    std::cout << "[EntryService] save entry: " << content << std::endl;
    eventBus.publish("EntryCreated", content);
}

} // namespace monggle
