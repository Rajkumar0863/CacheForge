#include "cacheforge/key_value_store.hpp"

#include <chrono>

namespace cacheforge {

KeyValueStore::KeyValueStore(std::size_t capacity)
    : capacity_(capacity) {
}

void KeyValueStore::set(
    const std::string& key,
    const std::string& value
) {
    // A normal SET removes any previous TTL from this key.
    expiry_.erase(key);

    auto it = data_.find(key);

    // Key already exists: update its value and mark it as recently used.
    if (it != data_.end()) {
        it->second.value = value;
        mark_as_recently_used(it);
        return;
    }

    // Add the new key to the most-recently-used end.
    lru_order_.push_back(key);

    auto lru_iterator = std::prev(lru_order_.end());

    data_.emplace(
        key,
        Entry{
            value,
            lru_iterator
        }
    );

    evict_if_needed();
}

void KeyValueStore::set_with_ttl(
    const std::string& key,
    const std::string& value,
    std::chrono::seconds ttl
) {
    auto it = data_.find(key);

    if (it != data_.end()) {
        // Update existing value.
        it->second.value = value;

        // Mark it as recently used.
        mark_as_recently_used(it);
    } else {
        // Insert new key into LRU list.
        lru_order_.push_back(key);

        auto lru_iterator = std::prev(lru_order_.end());

        data_.emplace(
            key,
            Entry{
                value,
                lru_iterator
            }
        );
    }

    // Store the expiration time.
    expiry_[key] = Clock::now() + ttl;

    evict_if_needed();
}

std::optional<std::string>
KeyValueStore::get(const std::string& key) {
    auto it = data_.find(key);

    if (it == data_.end()) {
        return std::nullopt;
    }

    // If the key has expired, remove it completely.
    if (is_expired(key)) {
        erase_key(key);
        return std::nullopt;
    }

    // Reading a key counts as using it.
    mark_as_recently_used(it);

    return it->second.value;
}

bool KeyValueStore::remove(const std::string& key) {
    auto it = data_.find(key);

    if (it == data_.end()) {
        return false;
    }

    // Remove from LRU tracking.
    lru_order_.erase(it->second.lru_iterator);

    // Remove TTL information if present.
    expiry_.erase(key);

    // Remove actual entry.
    data_.erase(it);

    return true;
}

bool KeyValueStore::contains(const std::string& key) {
    auto it = data_.find(key);

    if (it == data_.end()) {
        return false;
    }

    if (is_expired(key)) {
        erase_key(key);
        return false;
    }

    return true;
}

std::size_t KeyValueStore::size() {
    // Remove expired entries before reporting the size.
    cleanup_expired();

    return data_.size();
}

std::size_t KeyValueStore::capacity() const {
    return capacity_;
}

void KeyValueStore::mark_as_recently_used(
    std::unordered_map<std::string, Entry>::iterator entry
) {
    // Move the existing node to the most-recently-used end.
    lru_order_.splice(
        lru_order_.end(),
        lru_order_,
        entry->second.lru_iterator
    );

    entry->second.lru_iterator = std::prev(lru_order_.end());
}

void KeyValueStore::evict_if_needed() {
    // First clear expired entries.
    cleanup_expired();

    // capacity == 0 means unlimited.
    if (capacity_ == 0) {
        return;
    }

    while (data_.size() > capacity_) {
        // Front contains the least recently used key.
        std::string key_to_evict = lru_order_.front();

        // Remove TTL metadata if it exists.
        expiry_.erase(key_to_evict);

        // Remove actual entry.
        data_.erase(key_to_evict);

        // Remove from LRU list.
        lru_order_.pop_front();
    }
}

bool KeyValueStore::is_expired(const std::string& key) const {
    auto expiry_it = expiry_.find(key);

    // No expiry entry means this key lives indefinitely.
    if (expiry_it == expiry_.end()) {
        return false;
    }

    return Clock::now() >= expiry_it->second;
}

void KeyValueStore::erase_key(const std::string& key) {
    auto it = data_.find(key);

    if (it == data_.end()) {
        // Clean stale expiry information just in case.
        expiry_.erase(key);
        return;
    }

    // Remove from LRU list.
    lru_order_.erase(it->second.lru_iterator);

    // Remove TTL metadata.
    expiry_.erase(key);

    // Remove actual key-value entry.
    data_.erase(it);
}

void KeyValueStore::cleanup_expired() {
    auto it = expiry_.begin();

    while (it != expiry_.end()) {
        if (Clock::now() >= it->second) {
            const std::string key = it->first;

            auto data_it = data_.find(key);

            if (data_it != data_.end()) {
                lru_order_.erase(data_it->second.lru_iterator);
                data_.erase(data_it);
            }

            it = expiry_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace cacheforge