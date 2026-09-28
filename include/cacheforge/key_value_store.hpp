#pragma once

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
    // If the cache is full, the least recently used key is evicted.
    void set(const std::string& key, const std::string& value);

    // Return the value associated with the key.
    // Accessing a key marks it as recently used.
    // Returns std::nullopt if the key does not exist.
    std::optional<std::string> get(const std::string& key);

    // Remove a key from the store.
    // Returns true if the key existed and was removed.
    bool remove(const std::string& key);

    // Check whether a key exists in the store.
    bool contains(const std::string& key) const;

    // Return the number of key-value pairs currently stored.
    std::size_t size() const;

    // Return the maximum capacity of the cache.
    // 0 means unlimited capacity.
    std::size_t capacity() const;

private:
    struct Entry {
        std::string value;

        // Points to this key's position in the LRU list.
        std::list<std::string>::iterator lru_iterator;
    };

    // Move an existing key to the most-recently-used position.
    void mark_as_recently_used(
        std::unordered_map<std::string, Entry>::iterator entry);

    // Remove the least recently used key when capacity is reached.
    void evict_if_needed();

    // Maximum number of entries.
    // 0 means unlimited.
    std::size_t capacity_;

    // Hash table provides average O(1) key lookup.
    std::unordered_map<std::string, Entry> data_;

    // Tracks usage order.
    //
    // front = least recently used
    // back  = most recently used
    std::list<std::string> lru_order_;
};

} // namespace cacheforge