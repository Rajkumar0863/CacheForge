#pragma once

#include <chrono>
#include <cstddef>
#include <list>
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
    // A normal SET has no TTL.
    void set(
        const std::string& key,
        const std::string& value
    );

    // Insert or update a key with a Time-To-Live.
    // The key expires after the specified duration.
    void set_with_ttl(
        const std::string& key,
        const std::string& value,
        std::chrono::seconds ttl
    );

    // Return the value associated with the key.
    // Accessing a key marks it as recently used.
    // Returns std::nullopt if the key does not exist or has expired.
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

    // LRU ordering.
    // Front = least recently used.
    // Back = most recently used.
    std::list<std::string> lru_order_;

    // Expiration timestamps.
    // Keys without TTL are not stored here.
    std::unordered_map<
        std::string,
        Clock::time_point
    > expiry_;

    // Maximum cache capacity.
    // 0 means unlimited.
    std::size_t capacity_;

    // Move an entry to the most-recently-used position.
    void mark_as_recently_used(
        std::unordered_map<std::string, Entry>::iterator entry
    );

    // Evict least-recently-used entries when necessary.
    void evict_if_needed();

    // Check whether a key's TTL has expired.
    bool is_expired(
        const std::string& key
    ) const;

    // Completely remove a key from all internal structures.
    void erase_key(
        const std::string& key
    );

    // Remove all expired entries.
    void cleanup_expired();
};

} // namespace cacheforge