#pragma once

#include <memory>

namespace monggle {

class EntryService;

void configureRoutes(std::shared_ptr<EntryService> entryService);

} // namespace monggle
