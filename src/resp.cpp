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

// Strict non-negative integer parse for RESP length/count fields.
// Returns false on: empty input, any non-digit char (catches "3garbage"),
// or overflow. No exceptions — the caller maps false to a protocol ERROR.
static bool parseLen(const std::string& s, long long& out) {
    if (s.empty()) return false;
    long long val = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;     // rejects trailing junk, signs, spaces
        val = val * 10 + (c - '0');
        if (val > 1'000'000'000) return false;     // sanity cap — no real command is this big
    }
    out = val;
    return true;
}

ParseStatus parseCommand(const char* data, size_t len,
                         std::vector<std::string>& out, size_t& consumed) {
    size_t pos = 0;
    std::vector<std::string> result;
    std::string line;

    ParseStatus status = readLine(data, len, pos, line);
    if (status != ParseStatus::OK) return status;
    if (line.empty() || line[0] != '*') return ParseStatus::ERROR;

    long long count;
    if (!parseLen(line.substr(1), count)) return ParseStatus::ERROR;
    if (count < 1) return ParseStatus::ERROR;

    for (long long i = 0; i < count; i++) {
        status = readLine(data, len, pos, line);
        if (status != ParseStatus::OK) return status;
        if (line.empty() || line[0] != '$') return ParseStatus::ERROR;

        // NOTE: RESP permits a null bulk string ($-1) as an array element, but a null
        // in command position is malformed for our purposes, so parseLen rejects the
        // leading '-' and we treat it as a protocol ERROR. Intentional, not an oversight.
        long long strLen;
        if (!parseLen(line.substr(1), strLen)) return ParseStatus::ERROR;

        if (pos + static_cast<size_t>(strLen) + 2 > len) return ParseStatus::INCOMPLETE;

        result.push_back(std::string(data + pos, static_cast<size_t>(strLen)));
        pos += static_cast<size_t>(strLen) + 2;
    }

    consumed = pos;
    out = std::move(result);
    return ParseStatus::OK;
}

