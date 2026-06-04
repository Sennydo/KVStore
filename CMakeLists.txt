cmake_minimum_required(VERSION 3.14)
project(kvstore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(kvstore
    src/main.cpp
    src/server.cpp
    src/resp.cpp
)

add_executable(main
    src/main.cpp
)

target_include_directories(kvstore PRIVATE src)
target_include_directories(main PRIVATE src)

target_link_libraries(kvstore PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(main PRIVATE nlohmann_json::nlohmann_json)
