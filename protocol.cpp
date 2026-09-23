#include "protocol.h"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <sys/socket.h>
#include "socket.h"

Request parse_request_header(const std::string& header_line, int socket_fd)
{
    std::string line = header_line;

    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }

    if (line.empty()) {
        throw std::runtime_error("empty request");
    }

    std::istringstream iss(line);

    std::string op;
    if (!(iss >> op)) {
        throw std::runtime_error("missing operation");
    }

    for (char& c : op) {
        c = static_cast<char>(
            std::toupper(static_cast<unsigned char>(c))
        );
    }

    Request request;
    request.socket_fd = socket_fd;

    if (op == "HEALTH") {
        std::string extra;

        if (iss >> extra) {
            throw std::runtime_error("HEALTH takes no arguments");
        }

        request.op = "HEALTH";
        request.filename = "";
        request.bytes_to_transfer = 0;

        return request;
    }

    if (op == "GET") {
        std::string filename;
        std::string extra;

        if (!(iss >> filename)) {
            throw std::runtime_error("GET requires filename");
        }

        if (iss >> extra) {
            throw std::runtime_error("GET takes exactly one filename");
        }

        request.op = "GET";
        request.filename = filename;
        request.bytes_to_transfer = 0;

        return request;
    }

    if (op == "PUT") {
        std::string filename;
        std::string byte_string;
        std::string extra;

        if (!(iss >> filename)) {
            throw std::runtime_error("PUT requires filename");
        }

        if (!(iss >> byte_string)) {
            throw std::runtime_error("PUT requires byte count");
        }

        if (iss >> extra) {
            throw std::runtime_error(
                "PUT takes filename and byte count only"
            );
        }

        size_t bytes = 0;

        try {
            size_t consumed = 0;

            unsigned long long value =
                std::stoull(byte_string, &consumed);

            if (consumed != byte_string.size()) {
                throw std::runtime_error("invalid byte count");
            }

            bytes = static_cast<size_t>(value);
        }
        catch (...) {
            throw std::runtime_error("invalid byte count");
        }

        request.op = "PUT";
        request.filename = filename;
        request.bytes_to_transfer = bytes;

        return request;
    }

    throw std::runtime_error("unknown operation: " + op);
}

void send_response(int socket_fd, int value)
{
    std::string response = "OK " + std::to_string(value) + "\n";
    send_all(socket_fd, response.c_str(), response.size());
}

void send_error(int socket_fd, const std::string& reason)
{
    std::string response = "ERR " + reason + "\n";
    send_all(socket_fd, response.c_str(), response.size());
}


