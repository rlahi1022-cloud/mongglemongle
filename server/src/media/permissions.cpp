#include "monggle/media/permissions.h"

namespace monggle::permissions {

bool canView(std::int64_t viewerId,
             std::int64_t authorId,
             std::string_view visibility,
             bool isFollowerOfAuthor,
             bool isBlockedByAuthor) {
    if (viewerId == authorId) return true;
    if (isBlockedByAuthor)    return false;
    if (visibility == "public")  return true;
    if (visibility == "friends") return isFollowerOfAuthor;
    return false; // private
}

bool canDownload(std::int64_t viewerId,
                 std::int64_t authorId,
                 std::string_view visibility,
                 std::string_view downloadPolicy,
                 bool isFollowerOfAuthor,
                 bool isBlockedByAuthor) {
    if (viewerId == authorId) return true;
    if (!canView(viewerId, authorId, visibility, isFollowerOfAuthor, isBlockedByAuthor)) {
        return false;
    }
    if (downloadPolicy == "owner_only")     return false;
    if (downloadPolicy == "followers")      return isFollowerOfAuthor;
    if (downloadPolicy == "public_allowed") return visibility == "public";
    return false;
}

} // namespace monggle::permissions
