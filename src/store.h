#ifndef STORE_H
#define STORE_H

#include <unordered_map>
#include <string>
#include <optional>
#include <shared_mutex>
#include <array>


struct Shard {
    std::unordered_map<std::string, std::string> map;
    mutable std::shared_mutex mtx;
};

class Store {
    std::unordered_map<std::string, std::string> data;
    mutable std::shared_mutex mtx;

    //design decision - 16 shards. Power of 2
    static const size_t numShards = 16;
    std::array<Shard, numShards> shards;
    const Shard& shardFor(const std::string& key) const;
    Shard& shardFor(const std::string& key);

public:
    Store();
    std::optional<std::string> get(const std::string& key) const;
    void set(const std::string& key, const std::string& value);
    bool del(const std::string& key);

};

#endif // STORE_H