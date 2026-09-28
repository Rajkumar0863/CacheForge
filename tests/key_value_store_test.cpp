#include "cacheforge/key_value_store.hpp"

#include <gtest/gtest.h>

#include <string>

using cacheforge::KeyValueStore;

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

    store.set("project", "CacheForge");

    EXPECT_TRUE(store.contains("project"));
    EXPECT_FALSE(store.contains("missing"));
}

TEST(KeyValueStoreTest, RemoveExistingKey) {
    KeyValueStore store;

    store.set("name", "Rajkumar");

    EXPECT_TRUE(store.remove("name"));
    EXPECT_FALSE(store.contains("name"));
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


// ---------------------------------------------------------
// LRU CACHE TESTS
// ---------------------------------------------------------

TEST(KeyValueStoreTest, EvictsLeastRecentlyUsedEntry) {
    KeyValueStore store(3);

    store.set("A", "100");
    store.set("B", "200");
    store.set("C", "300");

    // A is currently the least recently used entry.
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

    // Access A, making it the most recently used.
    auto value = store.get("A");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "100");

    store.set("D", "400");

    // B should now be the least recently used.
    EXPECT_TRUE(store.contains("A"));
    EXPECT_FALSE(store.contains("B"));
    EXPECT_TRUE(store.contains("C"));
    EXPECT_TRUE(store.contains("D"));
    EXPECT_EQ(store.size(), 3);
}

TEST(KeyValueStoreTest, UpdatingValueUpdatesLRUOrder) {
    KeyValueStore store(3);

    store.set("A", "100");
    store.set("B", "200");
    store.set("C", "300");

    // Updating A should also make A recently used.
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