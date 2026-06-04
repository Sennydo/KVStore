// Linux based key-value store server implementation

#ifndef SERVER_H
#define SERVER_H
#include <string>
#include <unordered_map>

class Server {

private:
    int port;
    int server_fd; // File descriptor for the server socket

public:
    Server(int port);
    void start();
    void stop();
    void handleClient(int client_fd); // Function to handle client connections single thread

};

#endif // SERVER_H