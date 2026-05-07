#include "monggle/router/router.h"
#include <iostream>

namespace monggle {

void Router::addRoute(const std::string& path, const Handler& handler) {
    routes[path] = handler;
}

void Router::dispatch(const std::string& path, const std::string& token, const std::string& body) const {
    auto it = routes.find(path);
    if (it != routes.end()) {
        it->second(token, body);
    } else {
        std::cerr << "[Router] 404 " << path << std::endl;
    }
}

} // namespace monggle
