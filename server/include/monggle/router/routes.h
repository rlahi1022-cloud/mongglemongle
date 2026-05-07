#pragma once

#include <memory>

namespace monggle {

class AuthService;
class PostsService;

void configureHealthRoutes();
void configureAuthRoutes(std::shared_ptr<AuthService> authService);
void configurePostsRoutes(std::shared_ptr<AuthService> authService,
                          std::shared_ptr<PostsService> postsService);

} // namespace monggle
