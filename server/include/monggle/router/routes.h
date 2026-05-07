#pragma once

#include <memory>

namespace monggle {

class AuthService;
class PostsService;
class SnapshotService;
class FollowsService;
class MediaService;
class ProfileService;

void configureHealthRoutes();
void configureAuthRoutes(std::shared_ptr<AuthService> authService);
void configurePostsRoutes(std::shared_ptr<AuthService> authService,
                          std::shared_ptr<PostsService> postsService);
void configureSnapshotRoutes(std::shared_ptr<AuthService> authService,
                             std::shared_ptr<SnapshotService> snapshotService);
void configureFollowsRoutes(std::shared_ptr<AuthService> authService,
                            std::shared_ptr<FollowsService> followsService);
void configureMediaRoutes(std::shared_ptr<AuthService> authService,
                          std::shared_ptr<FollowsService> followsService,
                          std::shared_ptr<MediaService> mediaService,
                          std::string storageRoot);
void configureProfileRoutes(std::shared_ptr<AuthService> authService,
                            std::shared_ptr<ProfileService> profileService);

} // namespace monggle
