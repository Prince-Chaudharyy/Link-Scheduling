#pragma once

#include "request.h"
#include <string>

// Serve GET request - send file to client
void serve_get(const Request& req, const std::string& file_dir);

// Serve PUT request - receive file from client
void serve_put(const Request& req, const std::string& file_dir);

