#include "resp.h"
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

/*
General Notes:
- The parseCommand function is the main entry point for parsing RESP commands. It reads the first line to determine the type of RESP message and then processes it accordingly.

- ReadLine helper function parses a line from input string and checks for any errors

- For header, not tolerating anything for message N < 1
*/

ParseStatus readLine(const char* data, size_t len, size_t& pos, std::string& line) {

    for (size_t i = pos; i < len; i++) {
        if (data[i] == '\r') {

            if (i + 1 >= len) {
                return ParseStatus::INCOMPLETE;
            }

            if (data[i + 1] == '\n') {
                line = std::string(data + pos, i - pos);
                pos = i + 2; // Move past \r\n
                return ParseStatus::OK;
            } else {
                return ParseStatus::ERROR; // Invalid line ending
            }
        }
    }
    return ParseStatus::INCOMPLETE; // No line ending found
}

ParseStatus parseCommand(const char* data, size_t len, std::vector<std::string>& out, size_t& consumed) {
    size_t pos = 0;
    std::vector<std::string> result;
    std::string line;

    try {

        ParseStatus status = readLine(data, len, pos, line);
        if (status != ParseStatus::OK) return status;
        if (line.empty() || line[0] != '*') return ParseStatus::ERROR;

        int count = std::stoi(line.substr(1));
        if (count < 1) return ParseStatus::ERROR;


        for (int i = 0; i < count; i++) {
            status = readLine(data, len, pos, line);
            if (status != ParseStatus::OK) return status;
            if (line.empty() || line[0] != '$') return ParseStatus::ERROR;

            int strLen = std::stoi(line.substr(1));
            if (strLen < 0) return ParseStatus::ERROR;

            if (pos + strLen + 2 > len) return ParseStatus::INCOMPLETE;

            result.push_back(std::string(data + pos, strLen));
            pos += strLen + 2; 
        }
    } catch (const std::exception&) {
        return ParseStatus::ERROR; 
    }

    consumed = pos;
    out = std::move(result);
    return ParseStatus::OK;
}

