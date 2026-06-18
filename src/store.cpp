#include "store.h"
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <unordered_map>
#include <string>
#include <array>

Store::Store() = default;
// shared lock because get can be read by many workers whereas modifying is only by one worker

std::optional<std::string> Store::get(const std::string& key) const {

    const Shard& s = shardFor(key);
    std::shared_lock<std::shared_mutex> lock(s.mtx);
    auto it = s.map.find(key);
    if (it != s.map.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Store::set(const std::string& key, const std::string& value) {

    Shard& s = shardFor(key);
    std::unique_lock<std::shared_mutex> lock(s.mtx);
    s.map[key] = value;
    // std::unique_lock<std::shared_mutex> lock(mtx);
    // data[key] = value;
}

bool Store::del(const std::string& key) {

    Shard& s = shardFor(key);
    std::unique_lock<std::shared_mutex> lock(s.mtx);
    return s.map.erase(key) > 0;

    // std::unique_lock<std::shared_mutex> lock(mtx);
    // return data.erase(key) > 0;
}


// Sharding related

const Shard& Store::shardFor(const std::string& key) const {
    size_t hash = std::hash<std::string>{}(key);
    return shards[hash & (shards.size() - 1)];
}

//non const for setters
Shard& Store::shardFor(const std::string& key) {
    size_t hash = std::hash<std::string>{}(key);
    return shards[hash & (shards.size() - 1)];
}