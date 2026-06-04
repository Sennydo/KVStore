#ifndef STORE_H
#define STORE_H

#include <unordered_map>
#include <string>
#include <optional>

class Store {
    std::unordered_map<std::string, std::string> data;

public:
    std::optional<std::string> get(const std::string& key);
    void set(const std::string& key, const std::string& value);
    bool del(const std::string& key);

};

#endif // STORE_H