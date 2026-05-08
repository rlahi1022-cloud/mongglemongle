#include "monggle/posts/posts_service.h"
#include "monggle/ai/ai_hub_client.h"
#include "monggle/blocks/blocks_service.h"
#include "monggle/follows/follows_service.h"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>

#include <json/json.h>
#include <json/reader.h>
#include <json/writer.h>

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <sstream>
#include <utility>

namespace monggle {

const char* toDbString(Visibility v) {
    switch (v) {
        case Visibility::Public:  return "public";
        case Visibility::Friends: return "friends";
        case Visibility::Private: return "private";
    }
    return "private";
}

const char* toDbString(DownloadPolicy p) {
    switch (p) {
        case DownloadPolicy::OwnerOnly:      return "owner_only";
        case DownloadPolicy::Followers:      return "followers";
        case DownloadPolicy::PublicAllowed:  return "public_allowed";
    }
    return "owner_only";
}

const char* toDbString(PostCategory c) {
    switch (c) {
        case PostCategory::Feed:   return "feed";
        case PostCategory::Devlog: return "devlog";
    }
    return "feed";
}

std::optional<Visibility> parseVisibility(const std::string& s) {
    if (s == "public")  return Visibility::Public;
    if (s == "friends") return Visibility::Friends;
    if (s == "private") return Visibility::Private;
    return std::nullopt;
}

std::optional<DownloadPolicy> parseDownloadPolicy(const std::string& s) {
    if (s == "owner_only")      return DownloadPolicy::OwnerOnly;
    if (s == "followers")       return DownloadPolicy::Followers;
    if (s == "public_allowed")  return DownloadPolicy::PublicAllowed;
    return std::nullopt;
}

std::optional<PostCategory> parsePostCategory(const std::string& s) {
    if (s == "feed")   return PostCategory::Feed;
    if (s == "devlog") return PostCategory::Devlog;
    return std::nullopt;
}

namespace {

drogon::orm::DbClientPtr db() {
    return drogon::app().getDbClient("monggle_db");
}

Post rowToPost(const drogon::orm::Row& row) {
    Post p;
    p.id              = row["id"].as<std::int64_t>();
    p.userId          = row["user_id"].as<std::int64_t>();
    p.title           = row["title"].isNull() ? std::string{} : row["title"].as<std::string>();
    p.category        = parsePostCategory(row["category"].as<std::string>())
                            .value_or(PostCategory::Feed);
    p.body            = row["body"].as<std::string>();
    p.visibility      = parseVisibility(row["visibility"].as<std::string>())
                            .value_or(Visibility::Private);
    p.downloadPolicy  = parseDownloadPolicy(row["download_policy"].as<std::string>())
                            .value_or(DownloadPolicy::OwnerOnly);
    p.createdAt       = row["created_at"].as<std::string>();
    p.updatedAt       = row["updated_at"].as<std::string>();
    return p;
}

// 권한 매트릭스 (기획 8.2). 'friends' = follower 관계 (단방향).
bool canView(std::int64_t viewerId, const Post& p, FollowsService* follows, BlocksService* blocks) {
    if (viewerId == p.userId) return true;
    if (blocks && viewerId > 0 && blocks->isBlocked(viewerId, p.userId)) return false;
    switch (p.visibility) {
        case Visibility::Public:
            return true;
        case Visibility::Friends:
            // viewerId가 author(p.userId)를 follow 하면 통과
            return follows && viewerId > 0 && follows->isFollower(viewerId, p.userId);
        case Visibility::Private:
            return false;
    }
    return false;
}

constexpr const char* kEventCreated          = "created";
constexpr const char* kEventEdited           = "edited";
constexpr const char* kEventDeleted          = "deleted";
constexpr const char* kEventRestored         = "restored";
constexpr const char* kEventVisibilityChange = "visibility_changed";

std::string serializeJson(const Json::Value& v) {
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    return Json::writeString(b, v);
}

void insertEvent(const drogon::orm::DbClientPtr& tx,
                 std::int64_t userId,
                 std::int64_t postId,
                 const std::string& eventType,
                 const Json::Value& payload) {
    tx->execSqlSync(
        "INSERT INTO post_events (user_id, post_id, event_type, payload_json) "
        "VALUES (?, ?, ?, ?)",
        userId, postId, eventType, serializeJson(payload));
}

Json::Value vectorToJson(const std::vector<float>& vector) {
    Json::Value arr(Json::arrayValue);
    for (float v : vector) arr.append(v);
    return arr;
}

std::vector<float> parseVectorJson(const std::string& raw) {
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream in(raw);
    if (!Json::parseFromStream(builder, in, &root, &errs) || !root.isArray()) {
        return {};
    }
    std::vector<float> out;
    out.reserve(root.size());
    for (const auto& value : root) {
        if (!value.isNumeric()) return {};
        out.push_back(value.asFloat());
    }
    return out;
}

float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || b.empty() || a.size() != b.size()) return -1.0F;
    double dot = 0.0;
    double na = 0.0;
    double nb = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot += static_cast<double>(a[i]) * static_cast<double>(b[i]);
        na += static_cast<double>(a[i]) * static_cast<double>(a[i]);
        nb += static_cast<double>(b[i]) * static_cast<double>(b[i]);
    }
    if (na <= 0.0 || nb <= 0.0) return -1.0F;
    return static_cast<float>(dot / (std::sqrt(na) * std::sqrt(nb)));
}

std::optional<std::int64_t> storeEmbedding(const drogon::orm::DbClientPtr& tx,
                                           AiHubClient* aiHub,
                                           std::int64_t userId,
                                           std::int64_t postId,
                                           const std::string& text) {
    if (!aiHub) return std::nullopt;
    auto vector = aiHub->embed(text);
    if (!vector || vector->empty()) return std::nullopt;

    tx->execSqlSync("DELETE FROM embeddings WHERE post_id = ?", postId);
    auto inserted = tx->execSqlSync(
        "INSERT INTO embeddings (post_id, user_id, model_version, vector_json) "
        "VALUES (?, ?, ?, ?)",
        postId,
        userId,
        std::string("ai-hub"),
        serializeJson(vectorToJson(*vector)));
    return inserted.insertId();
}

} // namespace

PostsService::PostsService(std::shared_ptr<FollowsService> follows,
                           std::shared_ptr<BlocksService> blocks,
                           std::shared_ptr<AiHubClient> aiHub)
    : follows_(std::move(follows)), blocks_(std::move(blocks)), aiHub_(std::move(aiHub)) {}

Result<Post> PostsService::create(std::int64_t authorId, const CreatePostRequest& req) {
    if (req.body.empty()) {
        return PostsError{PostsError::BadRequest, "body must not be empty"};
    }
    // UTF-8 바이트 기준. 일반 피드는 짧게, 개발일지는 근거/초안이 길어질 수 있어 넉넉히 허용.
    const std::size_t maxBodyBytes =
        req.category == PostCategory::Devlog ? 60000 : 3000;
    if (req.body.size() > maxBodyBytes) {
        return PostsError{
            PostsError::BadRequest,
            req.category == PostCategory::Devlog
                ? "body too long (max ~20000 chars for devlog)"
                : "body too long (max ~1000 chars)"
        };
    }
    try {
        auto tx = db()->newTransaction();

        auto inserted = tx->execSqlSync(
            "INSERT INTO posts (user_id, title, category, body, visibility, download_policy) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            authorId,
            req.title,
            std::string(toDbString(req.category)),
            req.body,
            std::string(toDbString(req.visibility)),
            std::string(toDbString(req.downloadPolicy)));
        std::int64_t postId = inserted.insertId();

        Json::Value payload(Json::objectValue);
        payload["title"]           = req.title;
        payload["category"]        = toDbString(req.category);
        payload["body"]            = req.body;
        payload["visibility"]      = toDbString(req.visibility);
        payload["download_policy"] = toDbString(req.downloadPolicy);
        insertEvent(tx, authorId, postId, kEventCreated, payload);

        if (auto embeddingId = storeEmbedding(tx, aiHub_.get(), authorId, postId, req.title + "\n" + req.body)) {
            tx->execSqlSync("UPDATE posts SET embedding_id = ? WHERE id = ?", *embeddingId, postId);
        }

        auto rows = tx->execSqlSync(
            "SELECT id, user_id, title, category, body, visibility, download_policy, created_at, updated_at "
            "FROM posts WHERE id = ?", postId);
        if (rows.size() == 0) {
            return PostsError{PostsError::InternalError, "post vanished after insert"};
        }
        return rowToPost(rows[0]);
    } catch (const drogon::orm::DrogonDbException& e) {
        return PostsError{PostsError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return PostsError{PostsError::InternalError, e.what()};
    }
}

Result<Post> PostsService::get(std::int64_t viewerId, std::int64_t postId) {
    try {
        auto rows = db()->execSqlSync(
            "SELECT id, user_id, title, category, body, visibility, download_policy, created_at, updated_at "
            "FROM posts WHERE id = ? AND deleted_at IS NULL LIMIT 1",
            postId);
        if (rows.size() == 0) {
            return PostsError{PostsError::NotFound, "post not found"};
        }
        auto post = rowToPost(rows[0]);
        if (!canView(viewerId, post, follows_.get(), blocks_.get())) {
            return PostsError{PostsError::Forbidden, "no permission to view this post"};
        }
        return post;
    } catch (const std::exception& e) {
        return PostsError{PostsError::InternalError, e.what()};
    }
}

Result<Post> PostsService::update(std::int64_t authorId, std::int64_t postId,
                                  const UpdatePostRequest& req) {
    if (!req.body && !req.visibility && !req.downloadPolicy && !req.title) {
        return PostsError{PostsError::BadRequest, "at least one field required"};
    }
    try {
        auto tx = db()->newTransaction();

        auto rows = tx->execSqlSync(
            "SELECT id, user_id, body, visibility, download_policy "
            "FROM posts WHERE id = ? AND deleted_at IS NULL LIMIT 1 FOR UPDATE",
            postId);
        if (rows.size() == 0) {
            return PostsError{PostsError::NotFound, "post not found"};
        }
        if (rows[0]["user_id"].as<std::int64_t>() != authorId) {
            return PostsError{PostsError::Forbidden, "not the author"};
        }

        Visibility prevVisibility = parseVisibility(rows[0]["visibility"].as<std::string>())
                                        .value_or(Visibility::Private);

        if (req.title) {
            std::string t = *req.title;
            tx->execSqlSync(
                "UPDATE posts SET title = ? WHERE id = ?", t, postId);
            Json::Value payload(Json::objectValue);
            payload["title"] = t;
            insertEvent(tx, authorId, postId, kEventEdited, payload);
        }
        if (req.body) {
            tx->execSqlSync("UPDATE posts SET body = ? WHERE id = ?", *req.body, postId);
            Json::Value payload(Json::objectValue);
            payload["body"] = *req.body;
            insertEvent(tx, authorId, postId, kEventEdited, payload);
        }
        if (req.visibility && *req.visibility != prevVisibility) {
            tx->execSqlSync("UPDATE posts SET visibility = ? WHERE id = ?",
                            std::string(toDbString(*req.visibility)), postId);
            Json::Value payload(Json::objectValue);
            payload["from"] = toDbString(prevVisibility);
            payload["to"]   = toDbString(*req.visibility);
            insertEvent(tx, authorId, postId, kEventVisibilityChange, payload);
        }
        if (req.downloadPolicy) {
            tx->execSqlSync("UPDATE posts SET download_policy = ? WHERE id = ?",
                            std::string(toDbString(*req.downloadPolicy)), postId);
            // download_policy 변경은 별도 event_type 미정 (기획 enum 5종 한정). 추후 확장.
        }

        auto refreshed = tx->execSqlSync(
            "SELECT id, user_id, title, category, body, visibility, download_policy, created_at, updated_at "
            "FROM posts WHERE id = ?", postId);
        if ((req.title || req.body) && refreshed.size() > 0) {
            const auto p = rowToPost(refreshed[0]);
            if (auto embeddingId = storeEmbedding(tx, aiHub_.get(), authorId, postId, p.title + "\n" + p.body)) {
                tx->execSqlSync("UPDATE posts SET embedding_id = ? WHERE id = ?", *embeddingId, postId);
            }
        }
        return rowToPost(refreshed[0]);
    } catch (const drogon::orm::DrogonDbException& e) {
        return PostsError{PostsError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return PostsError{PostsError::InternalError, e.what()};
    }
}

Result<bool> PostsService::remove(std::int64_t authorId, std::int64_t postId) {
    try {
        auto tx = db()->newTransaction();

        auto rows = tx->execSqlSync(
            "SELECT user_id FROM posts WHERE id = ? AND deleted_at IS NULL LIMIT 1 FOR UPDATE",
            postId);
        if (rows.size() == 0) {
            return PostsError{PostsError::NotFound, "post not found"};
        }
        if (rows[0]["user_id"].as<std::int64_t>() != authorId) {
            return PostsError{PostsError::Forbidden, "not the author"};
        }

        tx->execSqlSync("UPDATE posts SET deleted_at = NOW(3) WHERE id = ?", postId);
        Json::Value payload(Json::objectValue);
        insertEvent(tx, authorId, postId, kEventDeleted, payload);

        return true;
    } catch (const drogon::orm::DrogonDbException& e) {
        return PostsError{PostsError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return PostsError{PostsError::InternalError, e.what()};
    }
}

Result<std::vector<Post>> PostsService::searchOwn(std::int64_t userId,
                                                   const std::string& query,
                                                   int limit) {
    if (query.empty()) {
        return PostsError{PostsError::BadRequest, "q parameter required"};
    }
    if (limit <= 0 || limit > 100) limit = 20;
    try {
        std::vector<Post> out;
        std::unordered_set<std::int64_t> seen;

        if (aiHub_) {
            if (auto queryVector = aiHub_->embed(query); queryVector && !queryVector->empty()) {
                auto rows = db()->execSqlSync(
                    "SELECT p.id, p.user_id, p.title, p.category, p.body, p.visibility, "
                    "p.download_policy, p.created_at, p.updated_at, e.vector_json "
                    "FROM posts p "
                    "JOIN embeddings e ON e.id = p.embedding_id "
                    "WHERE p.user_id = ? AND p.deleted_at IS NULL "
                    "ORDER BY p.id DESC LIMIT 200",
                    userId);

                struct ScoredPost {
                    Post post;
                    float score;
                };
                std::vector<ScoredPost> scored;
                scored.reserve(rows.size());
                for (const auto& row : rows) {
                    const auto candidateVector = parseVectorJson(row["vector_json"].as<std::string>());
                    const float score = cosineSimilarity(*queryVector, candidateVector);
                    if (score >= 0.25F) scored.push_back(ScoredPost{rowToPost(row), score});
                }
                std::sort(scored.begin(), scored.end(), [](const ScoredPost& a, const ScoredPost& b) {
                    if (a.score == b.score) return a.post.id > b.post.id;
                    return a.score > b.score;
                });
                for (const auto& item : scored) {
                    if (static_cast<int>(out.size()) >= limit) break;
                    if (seen.insert(item.post.id).second) out.push_back(item.post);
                }
            }
        }

        if (static_cast<int>(out.size()) >= limit) return out;

        // MariaDB는 한글 ngram FULLTEXT 미지원이라 LIKE 결과를 함께 사용한다.
        std::string like = "%" + query + "%";
        auto rows = db()->execSqlSync(
            "SELECT id, user_id, title, category, body, visibility, download_policy, created_at, updated_at "
            "FROM posts "
            "WHERE user_id = ? AND deleted_at IS NULL AND (title LIKE ? OR body LIKE ?) "
            "ORDER BY id DESC LIMIT ?",
            userId, like, like, limit);
        out.reserve(out.size() + rows.size());
        for (const auto& row : rows) {
            if (static_cast<int>(out.size()) >= limit) break;
            auto post = rowToPost(row);
            if (seen.insert(post.id).second) out.push_back(std::move(post));
        }
        return out;
    } catch (const std::exception& e) {
        return PostsError{PostsError::InternalError, e.what()};
    }
}

Result<Post> PostsService::restore(std::int64_t authorId, std::int64_t postId) {
    try {
        auto tx = db()->newTransaction();

        // deleted_at NOT NULL인(=삭제된) 본인 글만 복원 대상.
        auto rows = tx->execSqlSync(
            "SELECT user_id, deleted_at FROM posts WHERE id = ? LIMIT 1 FOR UPDATE",
            postId);
        if (rows.size() == 0) {
            return PostsError{PostsError::NotFound, "post not found"};
        }
        if (rows[0]["user_id"].as<std::int64_t>() != authorId) {
            return PostsError{PostsError::Forbidden, "not the author"};
        }
        if (rows[0]["deleted_at"].isNull()) {
            return PostsError{PostsError::BadRequest, "post is not deleted"};
        }

        tx->execSqlSync("UPDATE posts SET deleted_at = NULL WHERE id = ?", postId);
        Json::Value payload(Json::objectValue);
        insertEvent(tx, authorId, postId, kEventRestored, payload);

        auto refreshed = tx->execSqlSync(
            "SELECT id, user_id, title, category, body, visibility, download_policy, created_at, updated_at "
            "FROM posts WHERE id = ?", postId);
        return rowToPost(refreshed[0]);
    } catch (const drogon::orm::DrogonDbException& e) {
        return PostsError{PostsError::InternalError, e.base().what()};
    } catch (const std::exception& e) {
        return PostsError{PostsError::InternalError, e.what()};
    }
}

Result<TimelinePage> PostsService::timeline(std::int64_t viewerId, std::int64_t ownerId,
                                            std::optional<std::int64_t> cursor,
                                            int limit,
                                            std::optional<PostCategory> category) {
    if (limit <= 0 || limit > 100) limit = 20;
    try {
        const bool isOwner   = (viewerId == ownerId);
        if (!isOwner && blocks_ && viewerId > 0 && blocks_->isBlocked(viewerId, ownerId)) {
            return PostsError{PostsError::Forbidden, "blocked users cannot view this timeline"};
        }
        const bool isFriend  = !isOwner && follows_ && viewerId > 0
                                && follows_->isFollower(viewerId, ownerId);
        const int  fetchN    = limit + 1;  // 다음 cursor 판정용 +1

        std::ostringstream sql;
        sql << "SELECT id, user_id, title, category, body, visibility, download_policy, created_at, updated_at "
               "FROM posts WHERE user_id = ? AND deleted_at IS NULL ";
        if (category) {
            sql << " AND category = ? ";
        }
        if (!isOwner) {
            sql << (isFriend ? " AND visibility IN ('public','friends') "
                             : " AND visibility = 'public' ");
        }
        if (cursor) {
            sql << " AND id < ? ";
        }
        sql << " ORDER BY id DESC LIMIT ?";

        drogon::orm::Result rows = [&]() {
            if (category && cursor) {
                return db()->execSqlSync(sql.str(), ownerId, std::string(toDbString(*category)), *cursor, fetchN);
            }
            if (category) {
                return db()->execSqlSync(sql.str(), ownerId, std::string(toDbString(*category)), fetchN);
            }
            if (cursor) {
                return db()->execSqlSync(sql.str(), ownerId, *cursor, fetchN);
            }
            return db()->execSqlSync(sql.str(), ownerId, fetchN);
        }();

        TimelinePage page;
        for (const auto& row : rows) {
            page.items.push_back(rowToPost(row));
        }
        if (static_cast<int>(page.items.size()) > limit) {
            page.nextCursor = page.items[limit - 1].id;
            page.items.resize(limit);
        }
        return page;
    } catch (const std::exception& e) {
        return PostsError{PostsError::InternalError, e.what()};
    }
}

} // namespace monggle
