#ifndef STORE_H
#define STORE_H

#include <unordered_map>
#include <string>
#include <optional>
#include <shared_mutex>

class Store {
    std::unordered_map<std::string, std::string> data;
    mutable std::shared_mutex mtx;

public:
    Store();
    std::optional<std::string> get(const std::string& key) const;
    void set(const std::string& key, const std::string& value);
    bool del(const std::string& key);

};

#endif // STORE_H