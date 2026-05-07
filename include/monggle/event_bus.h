#pragma once

#include <string>

namespace monggle {

class EventBus {
public:
    void publish(const std::string& eventName, const std::string& payload);
};

} // namespace monggle
