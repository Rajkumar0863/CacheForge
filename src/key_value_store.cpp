#include "cacheforge/key_value_store.hpp"

#include <chrono>
#include <iterator>
#include <mutex>

namespace cacheforge {

KeyValueStore::KeyValueStore(std::size_t capacity)
    : capacity_(capacity) {
}

void KeyValueStore::set(
    const std::string& key,
    const std::string& value
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // A normal SET removes any previous TTL.
    expiry_.erase(key);

    auto it = data_.find(key);

    // Update an existing entry.
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
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = data_.find(key);

    if (it != data_.end()) {
        // Update existing value.
        it->second.value = value;

        // Updating also counts as using the key.
        mark_as_recently_used(it);
    } else {
        // Insert new key into the LRU list.
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

    // Store/update expiration timestamp.
    expiry_[key] = Clock::now() + ttl;

    evict_if_needed();
}

std::optional<std::string>
KeyValueStore::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = data_.find(key);

    if (it == data_.end()) {
        return std::nullopt;
    }

    // Expired entries behave as if they do not exist.
    if (is_expired(key)) {
        erase_key(key);
        return std::nullopt;
    }

    // GET changes the LRU ordering.
    mark_as_recently_used(it);

    return it->second.value;
}

bool KeyValueStore::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = data_.find(key);

    if (it == data_.end()) {
        return false;
    }

    // If the key has expired, remove it and report it as missing.
    if (is_expired(key)) {
        erase_key(key);
        return false;
    }

    erase_key(key);

    return true;
}

bool KeyValueStore::contains(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

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
    std::lock_guard<std::mutex> lock(mutex_);

    // Do not count expired entries.
    cleanup_expired();

    return data_.size();
}

std::size_t KeyValueStore::capacity() const {
    std::lock_guard<std::mutex> lock(mutex_);

    return capacity_;
}

void KeyValueStore::mark_as_recently_used(
    std::unordered_map<std::string, Entry>::iterator entry
) {
    // IMPORTANT:
    // mutex_ is already owned by the calling public method.

    lru_order_.splice(
        lru_order_.end(),
        lru_order_,
        entry->second.lru_iterator
    );

    entry->second.lru_iterator =
        std::prev(lru_order_.end());
}

void KeyValueStore::evict_if_needed() {
    // IMPORTANT:
    // Do not lock mutex_ here.
    // The caller already owns the lock.

    // Expired entries should be removed before
    // capacity-based eviction occurs.
    cleanup_expired();

    // capacity == 0 means unlimited.
    if (capacity_ == 0) {
        return;
    }

    while (data_.size() > capacity_) {
        // Front = least recently used.
        const std::string key_to_evict =
            lru_order_.front();

        expiry_.erase(key_to_evict);
        data_.erase(key_to_evict);
        lru_order_.pop_front();
    }
}

bool KeyValueStore::is_expired(
    const std::string& key
) const {
    // IMPORTANT:
    // Caller already owns mutex_.

    auto expiry_it = expiry_.find(key);

    // No TTL entry means the key does not expire.
    if (expiry_it == expiry_.end()) {
        return false;
    }

    return Clock::now() >= expiry_it->second;
}

void KeyValueStore::erase_key(
    const std::string& key
) {
    // IMPORTANT:
    // Caller already owns mutex_.

    auto it = data_.find(key);

    if (it == data_.end()) {
        expiry_.erase(key);
        return;
    }

    // Remove from LRU ordering.
    lru_order_.erase(
        it->second.lru_iterator
    );

    // Remove TTL metadata.
    expiry_.erase(key);

    // Remove actual entry.
    data_.erase(it);
}

void KeyValueStore::cleanup_expired() {
    // IMPORTANT:
    // Caller already owns mutex_.

    auto it = expiry_.begin();

    while (it != expiry_.end()) {
        if (Clock::now() >= it->second) {
            const std::string key = it->first;

            auto data_it = data_.find(key);

            if (data_it != data_.end()) {
                lru_order_.erase(
                    data_it->second.lru_iterator
                );

                data_.erase(data_it);
            }

            it = expiry_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace cacheforge