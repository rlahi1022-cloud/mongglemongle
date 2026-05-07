#pragma once

#include <functional>
#include <map>
#include <string>

namespace monggle {

class Router {
public:
    using Handler = std::function<void(const std::string&, const std::string&)>;

    void addRoute(const std::string& path, const Handler& handler);
    void dispatch(const std::string& path, const std::string& token, const std::string& body) const;

private:
    std::map<std::string, Handler> routes;
};

} // namespace monggle
