#pragma once

namespace monggle {

class Router;
class EntryService;

void configureRoutes(Router& router, EntryService& entryService);

} // namespace monggle
