#include "store.h"

std::optional<std::string> Store::get(const std::string& key) {
    auto it = data.find(key);
    if (it != data.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Store::set(const std::string& key, const std::string& value) {
    data[key] = value;
}

bool Store::del(const std::string& key) {
    return data.erase(key) > 0;
}