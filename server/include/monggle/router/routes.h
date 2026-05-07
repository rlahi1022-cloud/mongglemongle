#pragma once

#include <memory>

namespace monggle {

class AuthService;
class PostsService;
class SnapshotService;

void configureHealthRoutes();
void configureAuthRoutes(std::shared_ptr<AuthService> authService);
void configurePostsRoutes(std::shared_ptr<AuthService> authService,
                          std::shared_ptr<PostsService> postsService);
void configureSnapshotRoutes(std::shared_ptr<AuthService> authService,
                             std::shared_ptr<SnapshotService> snapshotService);

} // namespace monggle
