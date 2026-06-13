#include "store.h"
#include <stdexcept>
#include <mutex>
#include <unordered_map>

Store::Store() = default;
// shared lock because get can be read by many workers whereas modifying is only by one worker

std::optional<std::string> Store::get(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mtx);
    auto it = data.find(key);
    if (it != data.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Store::set(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    data[key] = value;
}

bool Store::del(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    return data.erase(key) > 0;
}