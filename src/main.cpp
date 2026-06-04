#include <iostream>
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
    return 0;
}