#include "monggle/routes.h"
#include "monggle/entry_service.h"
#include "monggle/router.h"

namespace monggle {

void configureRoutes(Router& router, EntryService& entryService) {
    router.addRoute("/api/entry/create", [&entryService](const std::string& token, const std::string& body) {
        entryService.createEntry(token, body);
    });
    // TODO: add routes for snapshot restore, embedding search, download tasks, friend feed, media uploads
}

} // namespace monggle
