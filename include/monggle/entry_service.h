#pragma once

#include <string>

namespace monggle {

class AuthService;
class EventBus;

class EntryService {
public:
    EntryService(AuthService& auth, EventBus& bus);
    void createEntry(const std::string& userToken, const std::string& content);

private:
    AuthService& authService;
    EventBus& eventBus;
};

} // namespace monggle
