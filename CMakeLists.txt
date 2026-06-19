cmake_minimum_required(VERSION 3.14)
project(kvstore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# std::thread / std::shared_mutex need the system threading library (-pthread).
find_package(Threads REQUIRED)

# --- main server binary (README calls this ./build/kv) ---
add_executable(kv
    src/main.cpp
    src/server.cpp
    src/resp.cpp
    src/store.cpp
)
target_include_directories(kv PRIVATE src)
target_link_libraries(kv PRIVATE Threads::Threads)

# --- store microbenchmark ---
# bench.cpp is self-contained (defines its own ShardedStore and main), so it
# does not compile in the server/resp/store sources.
add_executable(benchmark_store
    tests/bench.cpp
)
target_link_libraries(benchmark_store PRIVATE Threads::Threads)

# --- test suite ---
enable_testing()
add_executable(tests
    tests/test_server.cpp
    src/server.cpp
    src/resp.cpp
    src/store.cpp
)
target_include_directories(tests PRIVATE src)
target_link_libraries(tests PRIVATE Threads::Threads)
add_test(NAME server_tests COMMAND tests)