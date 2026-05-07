#include "monggle/middleware/request_log.h"

#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>

namespace monggle {

void installRequestLog() {
    drogon::app().registerPreSendingAdvice(
        [](const drogon::HttpRequestPtr& req,
           const drogon::HttpResponsePtr& resp) {
            const auto& q = req->getQuery();
            LOG_INFO << req->getMethodString() << " "
                     << req->getPath()
                     << (q.empty() ? std::string{} : ("?" + q))
                     << " -> " << static_cast<int>(resp->getStatusCode())
                     << " peer=" << req->getPeerAddr().toIp();
        });
}

} // namespace monggle
