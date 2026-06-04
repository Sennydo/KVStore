#pragma once
#ifndef RESP_H
#define RESP_H

#include <string>
#include <vector>
#include <optional>

// Resp style parser
enum class ParseStatus {
    OK,
    INCOMPLETE,
    ERROR
};

//Looks at first byte and determines the type of RESP message, then parses accordingly
ParseStatus parseCommand(const std::string& request, size_t &pos, std::vector<std::string>&out, size_t &consumed);

std::string encodeSimpleString(const std::string& str) {
    return "+" + str + "\r\n";
}

std::string encodeError(const std::string& str) {
    return "-" + str + "\r\n";
}

std::string encodeBulkString(const std::string& str) {
    return "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n";
}

std::string encodeInteger(int64_t num) {
    return ":" + std::to_string(num) + "\r\n";
}

std::string encodeBulkNullString() {
    return "$-1\r\n";
}



#endif // RESP_H