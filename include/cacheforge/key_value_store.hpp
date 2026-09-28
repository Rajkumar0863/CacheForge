#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace cacheforge {

class KeyValueStore {
public:
    // Insert a new key-value pair or update an existing key.
    void set(const std::string& key, const std::string& value);

    // Return the value associated with the key.
    // Returns std::nullopt if the key does not exist.
    std::optional<std::string> get(const std::string& key) const;

    // Remove a key from the store.
    // Returns true if the key existed and was removed.
    bool remove(const std::string& key);

    // Check whether a key exists in the store.
    bool contains(const std::string& key) const;

    // Return the number of key-value pairs currently stored.
    std::size_t size() const;

private:
    // Hash table used as the underlying in-memory storage.
    std::unordered_map<std::string, std::string> data_;
};

} // namespace cacheforge