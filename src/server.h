#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "store.h"

class Server {
public:
    Server(int port, size_t numWorkers);
    ~Server();

    void start();   // binds, listens, runs the accept loop (blocks)
    void stop();    // closes the listening socket

private:
    // --- networking ---
    int port;
    int server_fd;
    void handleClient(int client_fd);

    // --- thread pool ---
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stopping = false;

    //Storage
    Store store;

    void workerLoop();
    void submit(std::function<void()> task);
    std::string dispatch(const std::vector<std::string>& cmd, Store& store);
};