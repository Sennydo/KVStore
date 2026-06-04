#include "server.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace std;

Server::Server(int port) : port(port), server_fd(-1) {
    // Constructor implementation
    std::cout << "Server initialized on port " << port << std::endl;
}

void Server::start() {
    // Start the server
    std::cout << "Server started on port " << port << std::endl;
    // Here you would add code to create a socket, bind it to the port, and listen for incoming connections
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind socket" << std::endl;
        return;
    }

    if (listen(server_fd, 3) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        return;
    }

    cout << "Server is listening for connections on port " << port << "..." << std::endl;

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }
        cout << "Accepted a new connection" << std::endl;
        // Here you would add code to handle the client connection, read requests, and send responses

        //single thread handle
        handleClient(client_fd);
        close(client_fd);
    }

}

void Server::stop() {
    // Stop the server
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
        std::cout << "Server stopped" << std::endl;
    }
}

void Server::handleClient(int client_fd) {
    // Handle client connection
    std::cout << "Handling client connection" << std::endl;
    // Here you would add code to read requests from the client, process them, and send responses
    
}

Server::~Server() {
    // Destructor implementation
    stop();
}