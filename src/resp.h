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
ParseStatus parseCommand(const char* data, size_t len, std::vector<std::string>& out, size_t& consumed);

std::string encodeSimpleString(const std::string& str);

std::string encodeError(const std::string& str);

std::string encodeBulkString(const std::string& str);

std::string encodeInteger(int64_t num);

std::string encodeBulkNullString();

ParseStatus readLine(const char* data, size_t len, size_t& pos, std::string& line);



#endif // RESP_H