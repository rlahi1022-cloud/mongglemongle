#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace monggle {

class FollowsService;
class BlocksService;

enum class Visibility { Public, Friends, Private };
enum class DownloadPolicy { OwnerOnly, Followers, PublicAllowed };
enum class PostCategory { Feed, Devlog };

const char* toDbString(Visibility v);
const char* toDbString(DownloadPolicy p);
const char* toDbString(PostCategory c);
std::optional<Visibility>     parseVisibility(const std::string& s);
std::optional<DownloadPolicy> parseDownloadPolicy(const std::string& s);
std::optional<PostCategory>   parsePostCategory(const std::string& s);

struct Post {
    std::int64_t   id;
    std::int64_t   userId;
    std::string    title;         // 빈 문자열이면 제목 미설정
    PostCategory   category;
    std::string    body;
    Visibility     visibility;
    DownloadPolicy downloadPolicy;
    std::string    createdAt;     // MariaDB DATETIME(3) string
    std::string    updatedAt;
};

struct PostsError {
    enum Code { NotFound, Forbidden, BadRequest, InternalError };
    Code        code;
    std::string detail;
};

template <typename T>
using Result = std::variant<T, PostsError>;

struct CreatePostRequest {
    std::string    title;         // 선택, 빈 문자열 가능
    PostCategory   category = PostCategory::Feed;
    std::string    body;
    Visibility     visibility;
    DownloadPolicy downloadPolicy;
};

struct UpdatePostRequest {
    std::optional<std::string>    title;
    std::optional<std::string>    body;
    std::optional<Visibility>     visibility;
    std::optional<DownloadPolicy> downloadPolicy;
};

struct TimelinePage {
    std::vector<Post> items;
    std::optional<std::int64_t> nextCursor;  // 다음 페이지 시작 id (없으면 끝)
};

class PostsService {
public:
    explicit PostsService(std::shared_ptr<FollowsService> follows = nullptr,
                          std::shared_ptr<BlocksService> blocks = nullptr);

    Result<Post> create(std::int64_t authorId, const CreatePostRequest& req);

    // viewerId == -1 이면 비로그인 (public만 통과)
    Result<Post> get(std::int64_t viewerId, std::int64_t postId);

    Result<Post> update(std::int64_t authorId, std::int64_t postId,
                        const UpdatePostRequest& req);

    Result<bool> remove(std::int64_t authorId, std::int64_t postId);

    // 삭제된(soft-delete) 본인 글을 복원. post_events에 'restored' 누적.
    Result<Post> restore(std::int64_t authorId, std::int64_t postId);

    // ownerId의 글을 viewerId 시점으로. cursor=nullopt 면 가장 최신부터.
    Result<TimelinePage> timeline(std::int64_t viewerId, std::int64_t ownerId,
                                  std::optional<std::int64_t> cursor,
                                  int limit,
                                  std::optional<PostCategory> category = std::nullopt);

    // 본인 글 키워드 검색 (FULLTEXT). MVP — 임베딩 검색은 AI 허브 후속.
    Result<std::vector<Post>> searchOwn(std::int64_t userId,
                                        const std::string& query,
                                        int limit);

private:
    std::shared_ptr<FollowsService> follows_;
    std::shared_ptr<BlocksService> blocks_;
};

} // namespace monggle
