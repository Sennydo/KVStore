#include <csignal>
#include <thread>
#include "server.h"

int main() {

    signal(SIGPIPE, SIG_IGN);

    unsigned workers = std::thread::hardware_concurrency();
    if (workers == 0) workers = 4;

    Server server(6380, workers);
    server.start();
    return 0;
}