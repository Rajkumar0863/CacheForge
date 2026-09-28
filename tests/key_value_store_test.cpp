#include "cacheforge/key_value_store.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <fstream>
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

    auto value = store.get("A");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "100");

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

    store.set(
        "session",
        "permanent"
    );

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

    auto value = store.get("A");

    ASSERT_TRUE(value.has_value());

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

    for (int i = 0; i < key_count; ++i) {
        store.set(
            "key_" + std::to_string(i),
            "initial"
        );
    }

    std::vector<std::thread> threads;

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

    start.store(true);

    writer.join();
    reader.join();
    remover.join();

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


// =========================================================
// PERSISTENCE TESTS
// =========================================================

TEST(KeyValueStoreTest, SaveAndLoadSingleEntry) {
    const std::string filename =
        "test_single.cache";

    {
        KeyValueStore store;

        store.set(
            "name",
            "Rajkumar"
        );

        ASSERT_TRUE(
            store.save(filename)
        );
    }

    {
        KeyValueStore store;

        ASSERT_TRUE(
            store.load(filename)
        );

        auto value =
            store.get("name");

        ASSERT_TRUE(
            value.has_value()
        );

        EXPECT_EQ(
            value.value(),
            "Rajkumar"
        );
    }

    std::remove(
        filename.c_str()
    );
}

TEST(KeyValueStoreTest, SaveAndLoadMultipleEntries) {
    const std::string filename =
        "test_multiple.cache";

    {
        KeyValueStore store;

        store.set("A", "100");
        store.set("B", "200");
        store.set("C", "300");

        ASSERT_TRUE(
            store.save(filename)
        );
    }

    {
        KeyValueStore store;

        ASSERT_TRUE(
            store.load(filename)
        );

        EXPECT_EQ(
            store.size(),
            3U
        );

        auto a = store.get("A");
        auto b = store.get("B");
        auto c = store.get("C");

        ASSERT_TRUE(a.has_value());
        ASSERT_TRUE(b.has_value());
        ASSERT_TRUE(c.has_value());

        EXPECT_EQ(a.value(), "100");
        EXPECT_EQ(b.value(), "200");
        EXPECT_EQ(c.value(), "300");
    }

    std::remove(
        filename.c_str()
    );
}

TEST(KeyValueStoreTest, LoadReplacesExistingState) {
    const std::string filename =
        "test_replace.cache";

    {
        KeyValueStore source;

        source.set(
            "disk_key",
            "disk_value"
        );

        ASSERT_TRUE(
            source.save(filename)
        );
    }

    KeyValueStore store;

    store.set(
        "memory_key",
        "memory_value"
    );

    ASSERT_TRUE(
        store.load(filename)
    );

    EXPECT_FALSE(
        store.contains("memory_key")
    );

    EXPECT_TRUE(
        store.contains("disk_key")
    );

    EXPECT_EQ(
        store.size(),
        1U
    );

    auto value =
        store.get("disk_key");

    ASSERT_TRUE(
        value.has_value()
    );

    EXPECT_EQ(
        value.value(),
        "disk_value"
    );

    std::remove(
        filename.c_str()
    );
}

TEST(KeyValueStoreTest, LoadMissingFileReturnsFalse) {
    const std::string filename =
        "cacheforge_file_that_does_not_exist.cache";

    std::remove(
        filename.c_str()
    );

    KeyValueStore store;

    EXPECT_FALSE(
        store.load(filename)
    );
}

TEST(KeyValueStoreTest, FailedLoadDoesNotDestroyExistingState) {
    const std::string filename =
        "test_corrupt.cache";

    {
        std::ofstream file(filename);

        file
            << "THIS_IS_NOT_A_CACHEFORGE_FILE\n"
            << "garbage\n";
    }

    KeyValueStore store;

    store.set(
        "existing",
        "value"
    );

    EXPECT_FALSE(
        store.load(filename)
    );

    EXPECT_TRUE(
        store.contains("existing")
    );

    auto value =
        store.get("existing");

    ASSERT_TRUE(
        value.has_value()
    );

    EXPECT_EQ(
        value.value(),
        "value"
    );

    std::remove(
        filename.c_str()
    );
}

TEST(KeyValueStoreTest, PersistenceSupportsSpaces) {
    const std::string filename =
        "test_spaces.cache";

    {
        KeyValueStore store;

        store.set(
            "full name",
            "Rajkumar Vijayan"
        );

        store.set(
            "message",
            "CacheForge persistence works"
        );

        ASSERT_TRUE(
            store.save(filename)
        );
    }

    {
        KeyValueStore store;

        ASSERT_TRUE(
            store.load(filename)
        );

        auto name =
            store.get("full name");

        auto message =
            store.get("message");

        ASSERT_TRUE(
            name.has_value()
        );

        ASSERT_TRUE(
            message.has_value()
        );

        EXPECT_EQ(
            name.value(),
            "Rajkumar Vijayan"
        );

        EXPECT_EQ(
            message.value(),
            "CacheForge persistence works"
        );
    }

    std::remove(
        filename.c_str()
    );
}

TEST(KeyValueStoreTest, PersistencePreservesLRUOrder) {
    const std::string filename =
        "test_lru.cache";

    {
        KeyValueStore store(3);

        store.set("A", "100");
        store.set("B", "200");
        store.set("C", "300");

        // LRU order after inserts:
        //
        // A -> B -> C
        //
        // Access A:
        //
        // B -> C -> A
        auto value =
            store.get("A");

        ASSERT_TRUE(
            value.has_value()
        );

        ASSERT_TRUE(
            store.save(filename)
        );
    }

    {
        KeyValueStore store(3);

        ASSERT_TRUE(
            store.load(filename)
        );

        // If LRU order was restored correctly,
        // B is still the least recently used.
        store.set(
            "D",
            "400"
        );

        EXPECT_TRUE(
            store.contains("A")
        );

        EXPECT_FALSE(
            store.contains("B")
        );

        EXPECT_TRUE(
            store.contains("C")
        );

        EXPECT_TRUE(
            store.contains("D")
        );

        EXPECT_EQ(
            store.size(),
            3U
        );
    }

    std::remove(
        filename.c_str()
    );
}

TEST(KeyValueStoreTest, LoadRespectsConfiguredCapacity) {
    const std::string filename =
        "test_capacity.cache";

    {
        KeyValueStore source;

        source.set("A", "100");
        source.set("B", "200");
        source.set("C", "300");
        source.set("D", "400");
        source.set("E", "500");

        ASSERT_TRUE(
            source.save(filename)
        );
    }

    {
        KeyValueStore store(3);

        ASSERT_TRUE(
            store.load(filename)
        );

        EXPECT_EQ(
            store.size(),
            3U
        );

        EXPECT_FALSE(
            store.contains("A")
        );

        EXPECT_FALSE(
            store.contains("B")
        );

        EXPECT_TRUE(
            store.contains("C")
        );

        EXPECT_TRUE(
            store.contains("D")
        );

        EXPECT_TRUE(
            store.contains("E")
        );
    }

    std::remove(
        filename.c_str()
    );
}

TEST(KeyValueStoreTest, TTLPersistsAcrossSaveAndLoad) {
    const std::string filename =
        "test_ttl.cache";

    {
        KeyValueStore store;

        store.set_with_ttl(
            "session",
            "abc123",
            std::chrono::seconds(3)
        );

        ASSERT_TRUE(
            store.save(filename)
        );
    }

    // Simulate CacheForge being stopped
    // for approximately one second.
    std::this_thread::sleep_for(
        std::chrono::milliseconds(1100)
    );

    {
        KeyValueStore store;

        ASSERT_TRUE(
            store.load(filename)
        );

        // Roughly two seconds of the original
        // TTL should still remain.
        EXPECT_TRUE(
            store.contains("session")
        );

        auto value =
            store.get("session");

        ASSERT_TRUE(
            value.has_value()
        );

        EXPECT_EQ(
            value.value(),
            "abc123"
        );

        // Wait beyond the remaining TTL.
        std::this_thread::sleep_for(
            std::chrono::milliseconds(2100)
        );

        EXPECT_FALSE(
            store.contains("session")
        );
    }

    std::remove(
        filename.c_str()
    );
}

TEST(KeyValueStoreTest, TTLCanExpireWhileCacheIsStopped) {
    const std::string filename =
        "test_ttl_expired.cache";

    {
        KeyValueStore store;

        store.set_with_ttl(
            "temporary",
            "value",
            std::chrono::seconds(1)
        );

        ASSERT_TRUE(
            store.save(filename)
        );
    }

    // Simulate CacheForge being stopped
    // longer than the remaining TTL.
    std::this_thread::sleep_for(
        std::chrono::milliseconds(1100)
    );

    {
        KeyValueStore store;

        ASSERT_TRUE(
            store.load(filename)
        );

        EXPECT_FALSE(
            store.contains("temporary")
        );

        EXPECT_EQ(
            store.size(),
            0U
        );
    }

    std::remove(
        filename.c_str()
    );
}