#include "monggle/cache/ttl_cache.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace monggle;

TEST(TtlCache, GetReturnsNulloptForMissingKey) {
    TtlCache c;
    EXPECT_FALSE(c.get("absent").has_value());
}

TEST(TtlCache, SetThenGet) {
    TtlCache c;
    c.set("k1", "v1", std::chrono::seconds(60));
    auto v = c.get("k1");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "v1");
}

TEST(TtlCache, ExpiredEntryEvictedOnRead) {
    TtlCache c;
    c.set("k", "v", std::chrono::seconds(0)); // 0s TTL → 즉시 만료
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_FALSE(c.get("k").has_value());
}

TEST(TtlCache, ExplicitInvalidate) {
    TtlCache c;
    c.set("a", "1", std::chrono::seconds(60));
    c.set("b", "2", std::chrono::seconds(60));
    c.invalidate("a");
    EXPECT_FALSE(c.get("a").has_value());
    EXPECT_TRUE(c.get("b").has_value());
}

TEST(TtlCache, PrefixInvalidateRemovesAllMatching) {
    TtlCache c;
    c.set("feed:7:0",  "x", std::chrono::seconds(60));
    c.set("feed:7:30", "y", std::chrono::seconds(60));
    c.set("feed:8:0",  "z", std::chrono::seconds(60));

    auto removed = c.invalidatePrefix("feed:7:");
    EXPECT_EQ(removed, 2u);
    EXPECT_FALSE(c.get("feed:7:0").has_value());
    EXPECT_FALSE(c.get("feed:7:30").has_value());
    EXPECT_TRUE (c.get("feed:8:0").has_value());
}

TEST(TtlCache, OverwriteResetsTtl) {
    TtlCache c;
    c.set("k", "old", std::chrono::seconds(0));
    c.set("k", "new", std::chrono::seconds(60));
    auto v = c.get("k");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "new");
}
