#include "server.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

Server::Server(int port, size_t numWorkers)
    : port(port), server_fd(-1) {
    for (size_t i = 0; i < numWorkers; ++i)
        workers.emplace_back([this] { workerLoop(); });
    std::cout << "Server initialized on port " << port
              << " with " << numWorkers << " workers\n";
}

Server::~Server() {
    stop();   // close listening socket if still open
    {
        std::unique_lock<std::mutex> lock(mtx);
        stopping = true;
    }
    cv.notify_all();
    for (auto& w : workers)
        if (w.joinable()) w.join();
}

void Server::submit(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mtx);
        tasks.push(std::move(task));
    }
    cv.notify_one();
}

void Server::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { return stopping || !tasks.empty(); });
            if (stopping && tasks.empty())
                return;
            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}

void Server::start() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Failed to create socket\n";
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind socket\n";
        return;
    }
    if (listen(server_fd, SOMAXCONN) < 0) {
        std::cerr << "Failed to listen on socket\n";
        return;
    }
    std::cout << "Listening on port " << port << "...\n";

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (server_fd == -1) break;   // stop() closed us; leave the loop
            std::cerr << "Failed to accept connection\n";
            continue;
        }
        std::cout << "Accepted connection (fd=" << client_fd << ")\n";
        submit([this, client_fd] {
            handleClient(client_fd);
            close(client_fd);
        });
    }
}

void Server::stop() {
    if (server_fd != -1) {
        shutdown(server_fd, SHUT_RDWR);
        close(server_fd);
        server_fd = -1;
        std::cout << "Server stopped\n";
    }
}

void Server::handleClient(int client_fd) {
    char buf[4096];
    while (true) {
        ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
        if (n <= 0) break;   // 0 = clean close, <0 = error
        send(client_fd, buf, n, 0);   // echo — RESP parser replaces this in Phase 1
    }
    std::cout << "Connection closed (fd=" << client_fd << ")\n";
}