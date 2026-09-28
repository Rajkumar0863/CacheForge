#include "cacheforge/key_value_store.hpp"

namespace cacheforge {

KeyValueStore::KeyValueStore(std::size_t capacity)
    : capacity_(capacity) {
}

void KeyValueStore::set(
    const std::string& key,
    const std::string& value
) {
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

std::optional<std::string>
KeyValueStore::get(const std::string& key) {
    auto it = data_.find(key);

    if (it == data_.end()) {
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

    // Remove the key from the LRU tracking list.
    lru_order_.erase(it->second.lru_iterator);

    // Remove the actual key-value entry.
    data_.erase(it);

    return true;
}

bool KeyValueStore::contains(const std::string& key) const {
    return data_.find(key) != data_.end();
}

std::size_t KeyValueStore::size() const {
    return data_.size();
}

std::size_t KeyValueStore::capacity() const {
    return capacity_;
}

void KeyValueStore::mark_as_recently_used(
    std::unordered_map<std::string, Entry>::iterator entry
) {
    // Move the existing list node directly to the end.
    // std::list::splice does this without copying the key.
    lru_order_.splice(
        lru_order_.end(),
        lru_order_,
        entry->second.lru_iterator
    );

    // Store the iterator to its new position.
    entry->second.lru_iterator = std::prev(lru_order_.end());
}

void KeyValueStore::evict_if_needed() {
    // capacity == 0 means unlimited.
    if (capacity_ == 0) {
        return;
    }

    while (data_.size() > capacity_) {
        // The front always contains the least recently used key.
        const std::string& key_to_evict = lru_order_.front();

        data_.erase(key_to_evict);
        lru_order_.pop_front();
    }
}

} // namespace cacheforge