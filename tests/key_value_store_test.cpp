#include "cacheforge/key_value_store.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

using cacheforge::KeyValueStore;


// =========================================================
// BASIC KEY-VALUE STORE TESTS
// =========================================================

TEST(KeyValueStoreTest, NewStoreIsEmpty) {
    KeyValueStore store;

    EXPECT_EQ(store.size(), 0U);
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

    EXPECT_EQ(store.size(), 1U);
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
    EXPECT_EQ(store.size(), 0U);
}

TEST(KeyValueStoreTest, RemoveMissingKey) {
    KeyValueStore store;

    EXPECT_FALSE(store.remove("missing"));
}

TEST(KeyValueStoreTest, SizeTracksEntries) {
    KeyValueStore store;

    EXPECT_EQ(store.size(), 0U);

    store.set("A", "100");
    EXPECT_EQ(store.size(), 1U);

    store.set("B", "200");
    EXPECT_EQ(store.size(), 2U);

    store.remove("A");
    EXPECT_EQ(store.size(), 1U);
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

    // A is the least recently used.
    store.set("D", "400");

    EXPECT_FALSE(store.contains("A"));
    EXPECT_TRUE(store.contains("B"));
    EXPECT_TRUE(store.contains("C"));
    EXPECT_TRUE(store.contains("D"));

    EXPECT_EQ(store.size(), 3U);
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

    // B should now be the least recently used.
    store.set("D", "400");

    EXPECT_TRUE(store.contains("A"));
    EXPECT_FALSE(store.contains("B"));
    EXPECT_TRUE(store.contains("C"));
    EXPECT_TRUE(store.contains("D"));

    EXPECT_EQ(store.size(), 3U);
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

    EXPECT_EQ(store.size(), 3U);

    EXPECT_TRUE(store.contains("A"));
    EXPECT_FALSE(store.contains("B"));
    EXPECT_TRUE(store.contains("C"));
    EXPECT_TRUE(store.contains("D"));
}

TEST(KeyValueStoreTest, ZeroCapacityMeansUnlimited) {
    KeyValueStore store;

    for (int i = 0; i < 100; ++i) {
        store.set(
            "key_" + std::to_string(i),
            "value_" + std::to_string(i)
        );
    }

    EXPECT_EQ(store.size(), 100U);
    EXPECT_EQ(store.capacity(), 0U);
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

    EXPECT_EQ(store.size(), 2U);

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1100)
    );

    EXPECT_EQ(store.size(), 1U);

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

    // Normal SET should remove the previous TTL.
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

    // Replace the old value and extend its TTL.
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

    // A becomes the most recently used.
    auto value = store.get("A");

    ASSERT_TRUE(value.has_value());

    // B should now be evicted.
    store.set("D", "400");

    EXPECT_TRUE(store.contains("A"));
    EXPECT_FALSE(store.contains("B"));
    EXPECT_TRUE(store.contains("C"));
    EXPECT_TRUE(store.contains("D"));

    EXPECT_EQ(store.size(), 3U);
}


// =========================================================
// THREAD SAFETY / CONCURRENCY TESTS
// =========================================================

TEST(KeyValueStoreTest, ConcurrentWriters) {
    KeyValueStore store;

    constexpr int thread_count = 8;
    constexpr int entries_per_thread = 500;

    std::vector<std::thread> threads;

    for (
        int thread_id = 0;
        thread_id < thread_count;
        ++thread_id
    ) {
        threads.emplace_back(
            [&store, thread_id]() {
                for (
                    int i = 0;
                    i < entries_per_thread;
                    ++i
                ) {
                    const std::string key =
                        "thread_"
                        + std::to_string(thread_id)
                        + "_key_"
                        + std::to_string(i);

                    const std::string value =
                        "value_"
                        + std::to_string(i);

                    store.set(key, value);
                }
            }
        );
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(
        store.size(),
        static_cast<std::size_t>(
            thread_count * entries_per_thread
        )
    );
}

TEST(KeyValueStoreTest, ConcurrentReadersAndWriters) {
    KeyValueStore store;

    constexpr int key_count = 1000;
    constexpr int writer_count = 4;
    constexpr int reader_count = 4;

    // Populate the store before concurrent access begins.
    for (int i = 0; i < key_count; ++i) {
        store.set(
            "key_" + std::to_string(i),
            "initial"
        );
    }

    std::vector<std::thread> threads;

    // Writers update existing keys.
    for (
        int writer = 0;
        writer < writer_count;
        ++writer
    ) {
        threads.emplace_back(
            [&store, writer]() {
                for (
                    int i = 0;
                    i < key_count;
                    ++i
                ) {
                    store.set(
                        "key_" + std::to_string(i),
                        "writer_"
                            + std::to_string(writer)
                    );
                }
            }
        );
    }

    // Readers access the same keys concurrently.
    for (
        int reader = 0;
        reader < reader_count;
        ++reader
    ) {
        threads.emplace_back(
            [&store]() {
                for (
                    int i = 0;
                    i < key_count;
                    ++i
                ) {
                    auto value = store.get(
                        "key_" + std::to_string(i)
                    );

                    EXPECT_TRUE(value.has_value());
                }
            }
        );
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(
        store.size(),
        static_cast<std::size_t>(key_count)
    );
}

TEST(KeyValueStoreTest, ConcurrentUpdatesToSameKey) {
    KeyValueStore store;

    store.set("shared", "initial");

    constexpr int thread_count = 8;
    constexpr int updates_per_thread = 1000;

    std::vector<std::thread> threads;

    for (
        int thread_id = 0;
        thread_id < thread_count;
        ++thread_id
    ) {
        threads.emplace_back(
            [&store, thread_id]() {
                for (
                    int i = 0;
                    i < updates_per_thread;
                    ++i
                ) {
                    store.set(
                        "shared",
                        "thread_"
                            + std::to_string(thread_id)
                            + "_"
                            + std::to_string(i)
                    );
                }
            }
        );
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(store.size(), 1U);
    EXPECT_TRUE(store.contains("shared"));

    auto value = store.get("shared");

    ASSERT_TRUE(value.has_value());
}

TEST(KeyValueStoreTest, ConcurrentSetGetAndRemove) {
    KeyValueStore store;

    constexpr int operation_count = 1000;

    // All workers wait until this becomes true.
    std::atomic<bool> start{false};

    std::thread writer(
        [&store, &start]() {
            while (!start.load()) {
                std::this_thread::yield();
            }

            for (
                int i = 0;
                i < operation_count;
                ++i
            ) {
                store.set(
                    "key_" + std::to_string(i),
                    "value_" + std::to_string(i)
                );
            }
        }
    );

    std::thread reader(
        [&store, &start]() {
            while (!start.load()) {
                std::this_thread::yield();
            }

            for (
                int i = 0;
                i < operation_count;
                ++i
            ) {
                store.get(
                    "key_" + std::to_string(i)
                );
            }
        }
    );

    std::thread remover(
        [&store, &start]() {
            while (!start.load()) {
                std::this_thread::yield();
            }

            for (
                int i = 0;
                i < operation_count;
                ++i
            ) {
                store.remove(
                    "key_" + std::to_string(i)
                );
            }
        }
    );

    // Release all three workers.
    start.store(true);

    writer.join();
    reader.join();
    remover.join();

    // Exact final size depends on thread scheduling.
    // The cache must remain internally valid.
    EXPECT_LE(
        store.size(),
        static_cast<std::size_t>(operation_count)
    );
}

TEST(KeyValueStoreTest, ConcurrentAccessRespectsCapacity) {
    constexpr std::size_t cache_capacity = 100;

    KeyValueStore store(cache_capacity);

    constexpr int thread_count = 8;
    constexpr int entries_per_thread = 500;

    std::vector<std::thread> threads;

    for (
        int thread_id = 0;
        thread_id < thread_count;
        ++thread_id
    ) {
        threads.emplace_back(
            [&store, thread_id]() {
                for (
                    int i = 0;
                    i < entries_per_thread;
                    ++i
                ) {
                    const std::string key =
                        "thread_"
                        + std::to_string(thread_id)
                        + "_key_"
                        + std::to_string(i);

                    const std::string value =
                        "value_"
                        + std::to_string(i);

                    store.set(key, value);
                }
            }
        );
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(
        store.size(),
        cache_capacity
    );
}

TEST(KeyValueStoreTest, ConcurrentTTLWrites) {
    KeyValueStore store;

    constexpr int thread_count = 4;
    constexpr int entries_per_thread = 100;

    std::vector<std::thread> threads;

    for (
        int thread_id = 0;
        thread_id < thread_count;
        ++thread_id
    ) {
        threads.emplace_back(
            [&store, thread_id]() {
                for (
                    int i = 0;
                    i < entries_per_thread;
                    ++i
                ) {
                    const std::string key =
                        "ttl_"
                        + std::to_string(thread_id)
                        + "_key_"
                        + std::to_string(i);

                    store.set_with_ttl(
                        key,
                        "value",
                        std::chrono::seconds(10)
                    );
                }
            }
        );
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(
        store.size(),
        static_cast<std::size_t>(
            thread_count * entries_per_thread
        )
    );
}