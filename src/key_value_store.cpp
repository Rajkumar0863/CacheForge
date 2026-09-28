#include "cacheforge/key_value_store.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <string>

namespace cacheforge {

KeyValueStore::KeyValueStore(std::size_t capacity)
    : capacity_(capacity) {
}


// =========================================================
// BASIC KEY-VALUE OPERATIONS
// =========================================================

void KeyValueStore::set(
    const std::string& key,
    const std::string& value
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // A normal SET removes any existing TTL.
    expiry_.erase(key);

    auto it = data_.find(key);

    if (it != data_.end()) {
        it->second.value = value;
        mark_as_recently_used(it);
        return;
    }

    lru_order_.push_back(key);

    auto lru_iterator =
        std::prev(lru_order_.end());

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
        it->second.value = value;

        mark_as_recently_used(it);
    } else {
        lru_order_.push_back(key);

        auto lru_iterator =
            std::prev(lru_order_.end());

        data_.emplace(
            key,
            Entry{
                value,
                lru_iterator
            }
        );
    }

    expiry_[key] =
        Clock::now() + ttl;

    evict_if_needed();
}


std::optional<std::string>
KeyValueStore::get(
    const std::string& key
) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = data_.find(key);

    if (it == data_.end()) {
        return std::nullopt;
    }

    if (is_expired(key)) {
        erase_key(key);
        return std::nullopt;
    }

    mark_as_recently_used(it);

    return it->second.value;
}


bool KeyValueStore::remove(
    const std::string& key
) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = data_.find(key);

    if (it == data_.end()) {
        return false;
    }

    if (is_expired(key)) {
        erase_key(key);
        return false;
    }

    erase_key(key);

    return true;
}


bool KeyValueStore::contains(
    const std::string& key
) {
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

    cleanup_expired();

    return data_.size();
}


std::size_t KeyValueStore::capacity() const {
    std::lock_guard<std::mutex> lock(mutex_);

    return capacity_;
}


// =========================================================
// PERSISTENCE
// =========================================================

bool KeyValueStore::save(
    const std::string& filename
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Do not persist entries that have already expired.
    cleanup_expired();

    std::ofstream file(
        filename,
        std::ios::out | std::ios::trunc
    );

    if (!file.is_open()) {
        return false;
    }

    // Simple format version marker.
    file << "CACHEFORGE_V1\n";

    // Store the number of entries.
    file << data_.size() << '\n';

    const auto steady_now =
        Clock::now();

    const auto system_now =
        std::chrono::system_clock::now();

    // Save in LRU order so the ordering can also be restored.
    //
    // Front = least recently used
    // Back  = most recently used
    for (const auto& key : lru_order_) {
        auto data_it = data_.find(key);

        if (data_it == data_.end()) {
            continue;
        }

        bool has_ttl = false;
        std::int64_t expiration_unix_ms = 0;

        auto expiry_it = expiry_.find(key);

        if (expiry_it != expiry_.end()) {
            has_ttl = true;

            // Determine how much TTL remains according to
            // steady_clock.
            const auto remaining =
                expiry_it->second - steady_now;

            // Convert that remaining duration into an absolute
            // system-clock expiration time suitable for disk.
            const auto system_expiration =
                system_now
                + std::chrono::duration_cast<
                    std::chrono::system_clock::duration
                >(remaining);

            expiration_unix_ms =
                std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(
                    system_expiration.time_since_epoch()
                ).count();
        }

        // std::quoted allows keys and values containing spaces,
        // quotes, and other common characters to round-trip.
        file
            << std::quoted(key)
            << ' '
            << std::quoted(data_it->second.value)
            << ' '
            << (has_ttl ? 1 : 0)
            << ' '
            << expiration_unix_ms
            << '\n';

        if (!file) {
            return false;
        }
    }

    file.flush();

    return file.good();
}


bool KeyValueStore::load(
    const std::string& filename
) {
    // Read the file before locking the cache.
    //
    // This avoids holding mutex_ during file I/O.
    std::ifstream file(filename);

    if (!file.is_open()) {
        return false;
    }

    std::string magic;

    if (!std::getline(file, magic)) {
        return false;
    }

    if (magic != "CACHEFORGE_V1") {
        return false;
    }

    std::size_t entry_count = 0;

    if (!(file >> entry_count)) {
        return false;
    }

    struct LoadedEntry {
        std::string key;
        std::string value;
        bool has_ttl;
        std::int64_t expiration_unix_ms;
    };

    std::list<LoadedEntry> loaded_entries;

    for (
        std::size_t i = 0;
        i < entry_count;
        ++i
    ) {
        LoadedEntry entry;

        int ttl_flag = 0;

        if (!(
            file
            >> std::quoted(entry.key)
            >> std::quoted(entry.value)
            >> ttl_flag
            >> entry.expiration_unix_ms
        )) {
            return false;
        }

        if (
            ttl_flag != 0
            && ttl_flag != 1
        ) {
            return false;
        }

        entry.has_ttl =
            ttl_flag == 1;

        loaded_entries.push_back(
            std::move(entry)
        );
    }

    const auto system_now =
        std::chrono::system_clock::now();

    const auto steady_now =
        Clock::now();

    // File has been successfully parsed.
    // Now modify the actual cache atomically.
    std::lock_guard<std::mutex> lock(mutex_);

    clear_internal();

    for (const auto& loaded : loaded_entries) {
        // -------------------------------------------------
        // TTL handling
        // -------------------------------------------------

        std::optional<Clock::time_point>
            reconstructed_expiry;

        if (loaded.has_ttl) {
            const auto system_expiration =
                std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(
                        loaded.expiration_unix_ms
                    )
                );

            // If the key expired while CacheForge was stopped,
            // do not restore it.
            if (system_expiration <= system_now) {
                continue;
            }

            const auto remaining =
                system_expiration - system_now;

            reconstructed_expiry =
                steady_now
                + std::chrono::duration_cast<
                    Clock::duration
                >(remaining);
        }

        // -------------------------------------------------
        // Restore value + LRU position
        // -------------------------------------------------

        lru_order_.push_back(
            loaded.key
        );

        auto lru_iterator =
            std::prev(lru_order_.end());

        auto existing =
            data_.find(loaded.key);

        // A valid persistence file should not contain
        // duplicate keys. Treat duplicates defensively by
        // replacing the previous copy.
        if (existing != data_.end()) {
            lru_order_.erase(
                existing->second.lru_iterator
            );

            data_.erase(existing);

            lru_iterator =
                std::prev(lru_order_.end());
        }

        data_.emplace(
            loaded.key,
            Entry{
                loaded.value,
                lru_iterator
            }
        );

        if (reconstructed_expiry.has_value()) {
            expiry_[loaded.key] =
                reconstructed_expiry.value();
        }

        // Respect the capacity configured for this store.
        evict_if_needed();
    }

    return true;
}


// =========================================================
// INTERNAL LRU HELPERS
// =========================================================

void KeyValueStore::mark_as_recently_used(
    std::unordered_map<
        std::string,
        Entry
    >::iterator entry
) {
    // mutex_ must already be held.

    lru_order_.splice(
        lru_order_.end(),
        lru_order_,
        entry->second.lru_iterator
    );

    entry->second.lru_iterator =
        std::prev(lru_order_.end());
}


void KeyValueStore::evict_if_needed() {
    // mutex_ must already be held.

    cleanup_expired();

    if (capacity_ == 0) {
        return;
    }

    while (
        data_.size() > capacity_
    ) {
        const std::string key_to_evict =
            lru_order_.front();

        expiry_.erase(
            key_to_evict
        );

        data_.erase(
            key_to_evict
        );

        lru_order_.pop_front();
    }
}


// =========================================================
// INTERNAL TTL HELPERS
// =========================================================

bool KeyValueStore::is_expired(
    const std::string& key
) const {
    // mutex_ must already be held.

    auto expiry_it =
        expiry_.find(key);

    if (
        expiry_it == expiry_.end()
    ) {
        return false;
    }

    return Clock::now()
        >= expiry_it->second;
}


void KeyValueStore::erase_key(
    const std::string& key
) {
    // mutex_ must already be held.

    auto it =
        data_.find(key);

    if (it == data_.end()) {
        expiry_.erase(key);
        return;
    }

    lru_order_.erase(
        it->second.lru_iterator
    );

    expiry_.erase(key);

    data_.erase(it);
}


void KeyValueStore::cleanup_expired() {
    // mutex_ must already be held.

    const auto now =
        Clock::now();

    auto it =
        expiry_.begin();

    while (
        it != expiry_.end()
    ) {
        if (now >= it->second) {
            const std::string key =
                it->first;

            auto data_it =
                data_.find(key);

            if (
                data_it != data_.end()
            ) {
                lru_order_.erase(
                    data_it->second.lru_iterator
                );

                data_.erase(
                    data_it
                );
            }

            it =
                expiry_.erase(it);
        } else {
            ++it;
        }
    }
}


// =========================================================
// INTERNAL CACHE MANAGEMENT
// =========================================================

void KeyValueStore::clear_internal() {
    // mutex_ must already be held.

    data_.clear();
    lru_order_.clear();
    expiry_.clear();
}

} // namespace cacheforge