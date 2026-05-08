#include "monggle/media/permissions.h"

#include <gtest/gtest.h>

using namespace monggle::permissions;

namespace {

constexpr std::int64_t kAuthor   = 1;
constexpr std::int64_t kFollower = 2;
constexpr std::int64_t kStranger = 3;
constexpr std::int64_t kBlocked  = 4;
constexpr std::int64_t kAnon     = -1;

} // namespace

// ─── canView ────────────────────────────────────────────────────────────

TEST(CanView, OwnerAlwaysSees_AllVisibilities) {
    EXPECT_TRUE(canView(kAuthor, kAuthor, "public",  false, false));
    EXPECT_TRUE(canView(kAuthor, kAuthor, "friends", false, false));
    EXPECT_TRUE(canView(kAuthor, kAuthor, "private", false, false));
}

TEST(CanView, BlockedNeverSees) {
    EXPECT_FALSE(canView(kBlocked, kAuthor, "public",  false, true));
    EXPECT_FALSE(canView(kBlocked, kAuthor, "friends", true,  true));
    EXPECT_FALSE(canView(kBlocked, kAuthor, "private", false, true));
}

TEST(CanView, PublicVisible_ToEveryoneNotBlocked) {
    EXPECT_TRUE(canView(kFollower, kAuthor, "public", true,  false));
    EXPECT_TRUE(canView(kStranger, kAuthor, "public", false, false));
    EXPECT_TRUE(canView(kAnon,     kAuthor, "public", false, false));
}

TEST(CanView, FriendsRequiresFollow) {
    EXPECT_TRUE (canView(kFollower, kAuthor, "friends", true,  false));
    EXPECT_FALSE(canView(kStranger, kAuthor, "friends", false, false));
    EXPECT_FALSE(canView(kAnon,     kAuthor, "friends", false, false));
}

TEST(CanView, PrivateOnlyOwner) {
    EXPECT_FALSE(canView(kFollower, kAuthor, "private", true,  false));
    EXPECT_FALSE(canView(kStranger, kAuthor, "private", false, false));
}

// ─── canDownload ────────────────────────────────────────────────────────

TEST(CanDownload, OwnerCanAlways_RegardlessOfPolicy) {
    EXPECT_TRUE(canDownload(kAuthor, kAuthor, "public",  "owner_only",     false, false));
    EXPECT_TRUE(canDownload(kAuthor, kAuthor, "friends", "followers",      false, false));
    EXPECT_TRUE(canDownload(kAuthor, kAuthor, "private", "public_allowed", false, false));
}

TEST(CanDownload, OwnerOnly_BlocksEverybodyElse) {
    EXPECT_FALSE(canDownload(kFollower, kAuthor, "public",  "owner_only", true,  false));
    EXPECT_FALSE(canDownload(kStranger, kAuthor, "public",  "owner_only", false, false));
    EXPECT_FALSE(canDownload(kFollower, kAuthor, "friends", "owner_only", true,  false));
}

TEST(CanDownload, Followers_OnlyFollowers) {
    EXPECT_TRUE (canDownload(kFollower, kAuthor, "public",  "followers", true,  false));
    EXPECT_TRUE (canDownload(kFollower, kAuthor, "friends", "followers", true,  false));
    EXPECT_FALSE(canDownload(kStranger, kAuthor, "public",  "followers", false, false));
    // canView가 false면 무조건 false
    EXPECT_FALSE(canDownload(kStranger, kAuthor, "friends", "followers", false, false));
}

TEST(CanDownload, PublicAllowed_OnlyOnPublic) {
    EXPECT_TRUE (canDownload(kStranger, kAuthor, "public",  "public_allowed", false, false));
    // friends 글은 visibility가 public이 아니니 public_allowed여도 false
    EXPECT_FALSE(canDownload(kFollower, kAuthor, "friends", "public_allowed", true,  false));
}

TEST(CanDownload, BlockedAlwaysFalse) {
    EXPECT_FALSE(canDownload(kBlocked, kAuthor, "public", "public_allowed", false, true));
    EXPECT_FALSE(canDownload(kBlocked, kAuthor, "public", "followers",      true,  true));
}

// ─── 매트릭스 합성 — visibility × downloadPolicy × follow × block 모든 셀 검증 ──
TEST(Matrix, AllCells) {
    struct Case {
        std::string_view vis;
        std::string_view pol;
        bool isFollower;
        bool isBlocked;
        bool expectView;
        bool expectDownload;
    };

    // viewer != owner인 경우만. owner는 항상 true (위에서 검증됨).
    const Case cases[] = {
        // public + owner_only: 본다 / 다운 X
        {"public",  "owner_only",     false, false, true,  false},
        {"public",  "owner_only",     true,  false, true,  false},
        // public + followers: 본다 / follower면 다운
        {"public",  "followers",      false, false, true,  false},
        {"public",  "followers",      true,  false, true,  true},
        // public + public_allowed: 본다 / 다운 가능
        {"public",  "public_allowed", false, false, true,  true},
        // friends + 모든 정책: follower만 본다
        {"friends", "owner_only",     true,  false, true,  false},
        {"friends", "followers",      true,  false, true,  true},
        {"friends", "public_allowed", true,  false, true,  false}, // public이 아니므로 다운 X
        {"friends", "owner_only",     false, false, false, false},
        // private + 모든 정책: 본인 외 X
        {"private", "owner_only",     true,  false, false, false},
        // 차단되면 visibility 무관 false
        {"public",  "public_allowed", true,  true,  false, false},
    };

    for (const auto& c : cases) {
        SCOPED_TRACE(testing::Message()
                     << "vis=" << c.vis << " pol=" << c.pol
                     << " follower=" << c.isFollower << " blocked=" << c.isBlocked);
        EXPECT_EQ(canView(kStranger, kAuthor, c.vis, c.isFollower, c.isBlocked),
                  c.expectView);
        EXPECT_EQ(canDownload(kStranger, kAuthor, c.vis, c.pol, c.isFollower, c.isBlocked),
                  c.expectDownload);
    }
}
