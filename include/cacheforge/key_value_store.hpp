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

    // Insert a new key-value pair or update an existing key.
    // A normal SET removes any existing TTL.
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
    // Accessing a key updates its LRU position.
    // Returns std::nullopt if the key is missing or expired.
    std::optional<std::string> get(
        const std::string& key
    );

    // Remove a key.
    // Returns true if the key existed.
    bool remove(
        const std::string& key
    );

    // Check whether a key currently exists.
    // Expired keys are treated as missing.
    bool contains(
        const std::string& key
    );

    // Return the number of currently valid entries.
    std::size_t size();

    // Return the configured maximum capacity.
    // 0 means unlimited.
    std::size_t capacity() const;

private:
    using Clock = std::chrono::steady_clock;

    struct Entry {
        std::string value;
        std::list<std::string>::iterator lru_iterator;
    };

    // Main key-value storage.
    std::unordered_map<std::string, Entry> data_;

    // LRU ordering:
    // front = least recently used
    // back  = most recently used
    std::list<std::string> lru_order_;

    // Expiration timestamps for TTL-enabled keys.
    std::unordered_map<
        std::string,
        Clock::time_point
    > expiry_;

    // Maximum cache capacity.
    // 0 means unlimited.
    std::size_t capacity_;

    // Protects all shared cache state:
    // data_, lru_order_, expiry_, and capacity-related operations.
    mutable std::mutex mutex_;

    // -----------------------------------------------------
    // Internal helpers
    //
    // IMPORTANT:
    // These helpers DO NOT acquire mutex_ themselves.
    // They are called while the public method already owns
    // the lock. This prevents recursive locking/deadlocks.
    // -----------------------------------------------------

    // Move an entry to the most-recently-used position.
    void mark_as_recently_used(
        std::unordered_map<std::string, Entry>::iterator entry
    );

    // Evict least-recently-used entries when capacity is exceeded.
    void evict_if_needed();

    // Check whether a key has expired.
    bool is_expired(
        const std::string& key
    ) const;

    // Remove a key from all internal structures.
    void erase_key(
        const std::string& key
    );

    // Remove all expired entries.
    void cleanup_expired();
};

} // namespace cacheforge