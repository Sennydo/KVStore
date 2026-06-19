#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <random>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <deque>
#include <optional>
#include <mutex>

// A sharded store whose shard count is a runtime value, so one binary
// can sweep 1,2,4,8,16,... without recompiling. Real server uses a fixed
// compile-time SHARD_COUNT=16; the locking logic here matches exactly.
class ShardedStore {
public:
    explicit ShardedStore(size_t shardCount)
        : mask(shardCount - 1), shards(shardCount) {}

    std::optional<std::string> get(const std::string& key) {
        Shard& s = shardFor(key);
        std::shared_lock<std::shared_mutex> lock(s.mtx);
        auto it = s.map.find(key);
        if (it == s.map.end()) return std::nullopt;
        return it->second;
    }

    void set(const std::string& key, const std::string& value) {
        Shard& s = shardFor(key);
        std::unique_lock<std::shared_mutex> lock(s.mtx);
        s.map[key] = value;
    }

private:
    struct Shard {
        std::unordered_map<std::string, std::string> map;
        std::shared_mutex mtx;
    };
    size_t mask;                         // shardCount - 1, used as bitmask
    std::deque<Shard> shards;            // deque: shared_mutex is non-movable

    Shard& shardFor(const std::string& key) {
        return shards[std::hash<std::string>{}(key) & mask];
    }
};

struct Result {
    size_t shards;
    double opsPerSec;
};

// Each thread runs a fixed number of ops: a read-heavy mix (90% GET, 10% SET),
// mirroring a realistic cache workload, over a large keyspace so collisions
// land across shards rather than all on one.
Result runTrial(size_t shardCount, int threadCount, int opsPerThread, int keyspace) {
    ShardedStore store(shardCount);


    std::vector<std::string> keys;
    keys.reserve(keyspace);
    for (int i = 0; i < keyspace; ++i)
        keys.push_back("key:" + std::to_string(i));

    // Pre-populate so GETs hit.
    for (int i = 0; i < keyspace; ++i)
        store.set(keys[i], "v");

    std::atomic<bool> go{false};
    auto worker = [&](int seed) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> keyDist(0, keyspace - 1);
        std::uniform_int_distribution<int> opDist(0, 9);
        while (!go.load(std::memory_order_acquire)) { /* spin until start */ }
        for (int i = 0; i < opsPerThread; ++i) {
            const std::string& k = keys[keyDist(rng)];   // index, no allocation
            if (opDist(rng) == 0) store.set(k, "v");     // 10% writes
            else                  store.get(k);          // 90% reads
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < threadCount; ++t) threads.emplace_back(worker, t + 1);

    auto start = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    for (auto& th : threads) th.join();
    auto end = std::chrono::steady_clock::now();

    double secs = std::chrono::duration<double>(end - start).count();
    double totalOps = double(threadCount) * opsPerThread;
    return { shardCount, totalOps / secs };
}

int main(int argc, char** argv) {
    int threadCount  = (argc > 1) ? std::atoi(argv[1]) : 12;
    int opsPerThread = (argc > 2) ? std::atoi(argv[2]) : 2'000'000;
    int keyspace     = (argc > 3) ? std::atoi(argv[3]) : 100'000;

    std::cout << "Sharded-store scaling benchmark\n";
    std::cout << "threads=" << threadCount
              << "  ops/thread=" << opsPerThread
              << "  keyspace=" << keyspace
              << "  workload=90% GET / 10% SET\n\n";
    std::cout << std::left << std::setw(10) << "shards"
              << std::setw(18) << "throughput(ops/s)"
              << "speedup vs 1 shard\n";
    std::cout << "-----------------------------------------------\n";

    double baseline = 0;
    for (size_t shards : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, 1024u, 2048u, 4096u, 8192u}) {
        Result r = runTrial(shards, threadCount, opsPerThread, keyspace);
        if (shards == 1) baseline = r.opsPerSec;
        std::cout << std::left << std::setw(10) << r.shards
                  << std::setw(18) << std::fixed << std::setprecision(0) << r.opsPerSec
                  << std::setprecision(2) << (r.opsPerSec / baseline) << "x\n";
    }
    return 0;
}