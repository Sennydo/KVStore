#include <iostream>
#include <csignal>
#include "server.h"
#include <nlohmann/json.hpp>
using namespace std;
using json = nlohmann::json;

int main() {

    signal(SIGPIPE, SIG_IGN);

    int workers = 200;

    Server server(6380, workers);
    server.start();
    return 0;
}