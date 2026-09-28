#include "cacheforge/key_value_store.hpp"

#include <gtest/gtest.h>

using cacheforge::KeyValueStore;

// A newly created store should contain no entries.
TEST(KeyValueStoreTest, NewStoreIsEmpty) {
    KeyValueStore store;

    EXPECT_EQ(store.size(), 0);
}

// SET should insert a new key-value pair.
TEST(KeyValueStoreTest, SetAndGetValue) {
    KeyValueStore store;

    store.set("name", "Rajkumar");

    auto value = store.get("name");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "Rajkumar");
}

// SET should update the value when the key already exists.
TEST(KeyValueStoreTest, SetUpdatesExistingValue) {
    KeyValueStore store;

    store.set("language", "Java");
    store.set("language", "C++");

    auto value = store.get("language");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "C++");

    // Updating an existing key must not increase the number of entries.
    EXPECT_EQ(store.size(), 1);
}

// GET should return no value for a missing key.
TEST(KeyValueStoreTest, GetMissingKeyReturnsNullopt) {
    KeyValueStore store;

    auto value = store.get("missing");

    EXPECT_FALSE(value.has_value());
}

// CONTAINS should correctly report whether a key exists.
TEST(KeyValueStoreTest, ContainsExistingKey) {
    KeyValueStore store;

    store.set("university", "UL");

    EXPECT_TRUE(store.contains("university"));
    EXPECT_FALSE(store.contains("country"));
}

// DELETE should remove an existing key.
TEST(KeyValueStoreTest, RemoveExistingKey) {
    KeyValueStore store;

    store.set("name", "Rajkumar");

    EXPECT_TRUE(store.remove("name"));
    EXPECT_FALSE(store.contains("name"));
    EXPECT_FALSE(store.get("name").has_value());
    EXPECT_EQ(store.size(), 0);
}

// DELETE should report failure when the key does not exist.
TEST(KeyValueStoreTest, RemoveMissingKey) {
    KeyValueStore store;

    EXPECT_FALSE(store.remove("does-not-exist"));
}

// SIZE should track multiple inserted values.
TEST(KeyValueStoreTest, SizeTracksEntries) {
    KeyValueStore store;

    store.set("one", "1");
    store.set("two", "2");
    store.set("three", "3");

    EXPECT_EQ(store.size(), 3);

    store.remove("two");

    EXPECT_EQ(store.size(), 2);
}

// Empty strings are valid keys and values in the core storage API.
TEST(KeyValueStoreTest, SupportsEmptyStrings) {
    KeyValueStore store;

    store.set("", "");

    EXPECT_TRUE(store.contains(""));

    auto value = store.get("");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "");
}