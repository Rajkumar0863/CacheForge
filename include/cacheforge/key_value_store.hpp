#pragma once

#include <chrono>
#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace cacheforge {

class KeyValueStore {
public:
    // Create a key-value store with a maximum capacity.
    // A capacity of 0 means unlimited capacity.
    explicit KeyValueStore(std::size_t capacity = 0);

    // -----------------------------------------------------
    // BASIC KEY-VALUE OPERATIONS
    // -----------------------------------------------------

    // Insert a new key-value pair or update an existing key.
    // A normal SET removes any existing TTL from the key.
    void set(
        const std::string& key,
        const std::string& value
    );

    // Insert or update a key with a Time-To-Live.
    void set_with_ttl(
        const std::string& key,
        const std::string& value,
        std::chrono::seconds ttl
    );

    // Return the value associated with the key.
    //
    // Accessing a key updates its LRU position.
    // Returns std::nullopt if the key is missing or expired.
    std::optional<std::string> get(
        const std::string& key
    );

    // Remove a key.
    //
    // Returns true if the key existed and was removed.
    bool remove(
        const std::string& key
    );

    // Check whether a key currently exists.
    //
    // Expired keys are treated as missing.
    bool contains(
        const std::string& key
    );

    // Return the number of currently valid entries.
    std::size_t size();

    // Return the configured maximum capacity.
    //
    // 0 means unlimited.
    std::size_t capacity() const;


    // -----------------------------------------------------
    // PERSISTENCE
    // -----------------------------------------------------

    // Save the current cache contents to disk.
    //
    // Expired entries are not written.
    //
    // Returns true when the file was written successfully.
    bool save(
        const std::string& filename
    );

    // Load cache contents from disk.
    //
    // Existing cache contents are replaced by the contents
    // of the persistence file.
    //
    // Entries whose TTL expired while CacheForge was not
    // running are ignored.
    //
    // Returns true when the file was loaded successfully.
    bool load(
        const std::string& filename
    );


private:
    using Clock = std::chrono::steady_clock;

    // -----------------------------------------------------
    // CACHE ENTRY
    // -----------------------------------------------------

    struct Entry {
        std::string value;

        std::list<std::string>::iterator lru_iterator;
    };


    // -----------------------------------------------------
    // CACHE STATE
    // -----------------------------------------------------

    // Main key-value storage.
    std::unordered_map<
        std::string,
        Entry
    > data_;

    // LRU ordering.
    //
    // Front = least recently used.
    // Back  = most recently used.
    std::list<std::string> lru_order_;

    // Expiration timestamps for TTL-enabled keys.
    //
    // Keys without a TTL do not appear in this map.
    std::unordered_map<
        std::string,
        Clock::time_point
    > expiry_;

    // Maximum number of entries allowed in the cache.
    //
    // 0 means unlimited.
    std::size_t capacity_;

    // Protects all shared cache state.
    //
    // Public operations acquire this mutex before touching:
    //
    // data_
    // lru_order_
    // expiry_
    //
    // Internal helper functions assume the mutex is already
    // owned by their caller.
    mutable std::mutex mutex_;


    // -----------------------------------------------------
    // INTERNAL HELPERS
    // -----------------------------------------------------

    // Move an entry to the most-recently-used position.
    //
    // Caller must already hold mutex_.
    void mark_as_recently_used(
        std::unordered_map<
            std::string,
            Entry
        >::iterator entry
    );

    // Remove least-recently-used entries when the configured
    // capacity has been exceeded.
    //
    // Caller must already hold mutex_.
    void evict_if_needed();

    // Check whether a TTL-enabled key has expired.
    //
    // Caller must already hold mutex_.
    bool is_expired(
        const std::string& key
    ) const;

    // Completely remove a key from:
    //
    // data_
    // lru_order_
    // expiry_
    //
    // Caller must already hold mutex_.
    void erase_key(
        const std::string& key
    );

    // Remove all expired entries.
    //
    // Caller must already hold mutex_.
    void cleanup_expired();

    // Clear all cache data.
    //
    // Used internally when loading a persistence file.
    //
    // Caller must already hold mutex_.
    void clear_internal();
};

} // namespace cacheforge