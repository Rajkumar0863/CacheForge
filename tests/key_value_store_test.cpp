#include "cacheforge/key_value_store.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

using cacheforge::KeyValueStore;


// =========================================================
// BASIC KEY-VALUE STORE TESTS
// =========================================================

TEST(KeyValueStoreTest, NewStoreIsEmpty) {
    KeyValueStore store;

    EXPECT_EQ(store.size(), 0);
}

TEST(KeyValueStoreTest, SetAndGetValue) {
    KeyValueStore store;

    store.set("name", "Rajkumar");

    auto value = store.get("name");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "Rajkumar");
}

TEST(KeyValueStoreTest, GetMissingKeyReturnsNullopt) {
    KeyValueStore store;

    auto value = store.get("missing");

    EXPECT_FALSE(value.has_value());
}

TEST(KeyValueStoreTest, SetUpdatesExistingValue) {
    KeyValueStore store;

    store.set("language", "Java");
    store.set("language", "C++");

    auto value = store.get("language");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "C++");

    EXPECT_EQ(store.size(), 1);
}

TEST(KeyValueStoreTest, ContainsExistingKey) {
    KeyValueStore store;

    store.set("A", "100");

    EXPECT_TRUE(store.contains("A"));
    EXPECT_FALSE(store.contains("B"));
}

TEST(KeyValueStoreTest, RemoveExistingKey) {
    KeyValueStore store;

    store.set("A", "100");

    EXPECT_TRUE(store.remove("A"));
    EXPECT_FALSE(store.contains("A"));
    EXPECT_EQ(store.size(), 0);
}

TEST(KeyValueStoreTest, RemoveMissingKey) {
    KeyValueStore store;

    EXPECT_FALSE(store.remove("missing"));
}

TEST(KeyValueStoreTest, SizeTracksEntries) {
    KeyValueStore store;

    EXPECT_EQ(store.size(), 0);

    store.set("A", "100");
    EXPECT_EQ(store.size(), 1);

    store.set("B", "200");
    EXPECT_EQ(store.size(), 2);

    store.remove("A");
    EXPECT_EQ(store.size(), 1);
}

TEST(KeyValueStoreTest, SupportsEmptyStrings) {
    KeyValueStore store;

    store.set("", "");

    auto value = store.get("");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "");
}


// =========================================================
// LRU CACHE TESTS
// =========================================================

TEST(KeyValueStoreTest, EvictsLeastRecentlyUsedEntry) {
    KeyValueStore store(3);

    store.set("A", "100");
    store.set("B", "200");
    store.set("C", "300");

    store.set("D", "400");

    EXPECT_FALSE(store.contains("A"));
    EXPECT_TRUE(store.contains("B"));
    EXPECT_TRUE(store.contains("C"));
    EXPECT_TRUE(store.contains("D"));

    EXPECT_EQ(store.size(), 3);
}

TEST(KeyValueStoreTest, GetUpdatesLRUOrder) {
    KeyValueStore store(3);

    store.set("A", "100");
    store.set("B", "200");
    store.set("C", "300");

    // A becomes the most recently used.
    ASSERT_TRUE(store.get("A").has_value());

    store.set("D", "400");

    // B should now be the least recently used.
    EXPECT_TRUE(store.contains("A"));
    EXPECT_FALSE(store.contains("B"));
    EXPECT_TRUE(store.contains("C"));
    EXPECT_TRUE(store.contains("D"));
}

TEST(KeyValueStoreTest, UpdatingValueUpdatesLRUOrder) {
    KeyValueStore store(3);

    store.set("A", "100");
    store.set("B", "200");
    store.set("C", "300");

    // Updating A also counts as using A.
    store.set("A", "999");

    store.set("D", "400");

    EXPECT_TRUE(store.contains("A"));
    EXPECT_FALSE(store.contains("B"));
    EXPECT_TRUE(store.contains("C"));
    EXPECT_TRUE(store.contains("D"));

    auto value = store.get("A");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "999");
}

TEST(KeyValueStoreTest, RemoveMaintainsLRUState) {
    KeyValueStore store(3);

    store.set("A", "100");
    store.set("B", "200");
    store.set("C", "300");

    EXPECT_TRUE(store.remove("B"));

    store.set("D", "400");

    EXPECT_EQ(store.size(), 3);

    EXPECT_TRUE(store.contains("A"));
    EXPECT_FALSE(store.contains("B"));
    EXPECT_TRUE(store.contains("C"));
    EXPECT_TRUE(store.contains("D"));
}

TEST(KeyValueStoreTest, ZeroCapacityMeansUnlimited) {
    KeyValueStore store;

    for (int i = 0; i < 100; ++i) {
        store.set(
            "key" + std::to_string(i),
            "value" + std::to_string(i)
        );
    }

    EXPECT_EQ(store.size(), 100);
    EXPECT_EQ(store.capacity(), 0);
}


// =========================================================
// TTL TESTS
// =========================================================

TEST(KeyValueStoreTest, SetWithTTLStoresValue) {
    KeyValueStore store;

    store.set_with_ttl(
        "session",
        "abc123",
        std::chrono::seconds(10)
    );

    auto value = store.get("session");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "abc123");
}

TEST(KeyValueStoreTest, TTLValueExpires) {
    KeyValueStore store;

    store.set_with_ttl(
        "session",
        "abc123",
        std::chrono::seconds(1)
    );

    EXPECT_TRUE(store.contains("session"));

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1100)
    );

    EXPECT_FALSE(store.contains("session"));
    EXPECT_FALSE(store.get("session").has_value());
}

TEST(KeyValueStoreTest, ExpiredValueReducesSize) {
    KeyValueStore store;

    store.set("permanent", "value");

    store.set_with_ttl(
        "temporary",
        "value",
        std::chrono::seconds(1)
    );

    EXPECT_EQ(store.size(), 2);

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1100)
    );

    EXPECT_EQ(store.size(), 1);

    EXPECT_TRUE(store.contains("permanent"));
    EXPECT_FALSE(store.contains("temporary"));
}

TEST(KeyValueStoreTest, NormalSetDoesNotExpire) {
    KeyValueStore store;

    store.set("name", "Rajkumar");

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1100)
    );

    EXPECT_TRUE(store.contains("name"));

    auto value = store.get("name");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "Rajkumar");
}

TEST(KeyValueStoreTest, NormalSetRemovesPreviousTTL) {
    KeyValueStore store;

    store.set_with_ttl(
        "session",
        "temporary",
        std::chrono::seconds(1)
    );

    // Normal SET should convert the key back
    // into a non-expiring entry.
    store.set("session", "permanent");

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1100)
    );

    EXPECT_TRUE(store.contains("session"));

    auto value = store.get("session");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "permanent");
}

TEST(KeyValueStoreTest, TTLUpdateReplacesExistingTTL) {
    KeyValueStore store;

    store.set_with_ttl(
        "token",
        "first",
        std::chrono::seconds(1)
    );

    // Replace the value and extend its TTL.
    store.set_with_ttl(
        "token",
        "second",
        std::chrono::seconds(3)
    );

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1100)
    );

    auto value = store.get("token");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "second");
}

TEST(KeyValueStoreTest, TTLWorksWithLRUEviction) {
    KeyValueStore store(3);

    store.set_with_ttl(
        "A",
        "100",
        std::chrono::seconds(10)
    );

    store.set("B", "200");
    store.set("C", "300");

    // Access A so B becomes the least recently used.
    ASSERT_TRUE(store.get("A").has_value());

    store.set("D", "400");

    EXPECT_TRUE(store.contains("A"));
    EXPECT_FALSE(store.contains("B"));
    EXPECT_TRUE(store.contains("C"));
    EXPECT_TRUE(store.contains("D"));

    EXPECT_EQ(store.size(), 3);
}