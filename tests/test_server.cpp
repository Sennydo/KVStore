#include "server.h"
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define CHECK(cond) \
    do { if (!(cond)) { std::cerr << "FAIL: " #cond " (" << __FILE__ << ":" << __LINE__ << ")\n"; std::exit(1); } } while(0)

static int make_client(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(fd >= 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    CHECK(connect(fd, (sockaddr*)&addr, sizeof(addr)) == 0);
    return fd;
}

// RAII wrapper: starts server in a background thread, stops it on destruction
struct TestServer {
    Server server;
    std::thread thread;
    int port;

    explicit TestServer(int p) : server(p, 4), port(p) {
        thread = std::thread([this] { server.start(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ~TestServer() {
        server.stop();
        thread.join();
    }
};

void test_startup() {
    TestServer ts(19001);
    int fd = make_client(ts.port);
    close(fd);
    std::cout << "PASS test_startup\n";
}

void test_echo() {
    TestServer ts(19002);
    int fd = make_client(ts.port);

    std::string msg = "hello kvstore\n";
    send(fd, msg.data(), msg.size(), 0);

    char buf[64]{};
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    CHECK(n == (ssize_t)msg.size());
    CHECK(std::string(buf, n) == msg);

    close(fd);
    std::cout << "PASS test_echo\n";
}

void test_concurrent_clients() {
    TestServer ts(19003);
    const int N = 6;
    std::vector<std::thread> threads;
    std::atomic<int> passed{0};

    for (int i = 0; i < N; ++i) {
        threads.emplace_back([i, &passed, &ts] {
            int fd = make_client(ts.port);
            std::string msg = "client_" + std::to_string(i) + "\n";
            send(fd, msg.data(), msg.size(), 0);

            char buf[64]{};
            ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
            if (n == (ssize_t)msg.size() && std::string(buf, n) == msg)
                ++passed;
            close(fd);
        });
    }
    for (auto& t : threads) t.join();
    CHECK(passed == N);
    std::cout << "PASS test_concurrent_clients (" << N << " clients)\n";
}

int main() {
    test_startup();
    test_echo();
    test_concurrent_clients();
    std::cout << "All tests passed.\n";
    return 0;
}
