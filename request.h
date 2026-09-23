#pragma once

#include <string>
#include <vector>
#include <ctime>

struct Request {
    // Basic info
    std::string op;              // "GET" or "PUT"
    std::string filename;
    size_t bytes_to_transfer;
    int socket_fd;
    
    // File handling
    size_t bytes_offset = 0;     // Current position in file
    std::vector<uint8_t> buffered_bytes;  // Partial reads from socket
    
    // Timing (CLOCK_MONOTONIC)
    struct timespec arrival;     // When admitted to queue
    struct timespec start;       // When scheduling started
    struct timespec finish;      // When completed
    
    // Scheduling metrics
    int rounds = 1;              // Number of times scheduled
    size_t forfeited_bytes = 0;  // Lost allowance on preemption
    size_t deficit = 0;          // DRR only
    size_t bytes_transferred = 0;
    
    // Request ID for tracking
    int request_id = -1;
};

