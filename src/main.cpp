#include <iostream>
#include <csignal>
#include "server.h"
#include <nlohmann/json.hpp>
using namespace std;
using json = nlohmann::json;

int main() {
    cout << "Hello, World!" << endl;

    json ex1 = json::parse(R"(
        {
            "pi": 3.141,
            "happy": true
        }
        )");
    cout << ex1 << endl;

    signal(SIGPIPE, SIG_IGN);

    Server server(6380, 4);
    server.start();
    return 0;
}