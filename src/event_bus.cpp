#include "monggle/event_bus.h"
#include <iostream>

namespace monggle {

void EventBus::publish(const std::string& eventName, const std::string& payload) {
    std::cout << "[EventBus] publish " << eventName << " payload=" << payload << std::endl;
    // TODO: enqueue event into a durable worker queue for event sourcing
}

} // namespace monggle
