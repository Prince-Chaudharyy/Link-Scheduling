#include "server.h"
#include "file_io.h"
#include "protocol.h"
#include "socket.h"
#include <iostream>
#include <fstream>
#include <sys/socket.h>

void serve_get(const Request& req, const std::string& file_dir) {
    // Build full file path
    std::string filepath = file_dir + "/" + req.filename;
    
    // Check if file exists
    if (!file_exists(filepath)) {
        std::cerr << "File not found: " << filepath << std::endl;
        send_error(req.socket_fd, "file not found");
        return;
    }
    
    // Get file size
    size_t file_size = get_file_size(filepath);
    
    // Send OK response with file size
    std::cout << "  Sending " << file_size << " bytes from " << req.filename << std::endl;
    send_response(req.socket_fd, file_size);
    
    // Now send the actual file content
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filepath << std::endl;
        return;
    }
    
    // Read and send file in chunks
    const size_t CHUNK_SIZE = 8192;  // 8KB chunks
    char buffer[CHUNK_SIZE];
    
    while (file.good()) {
        file.read(buffer, CHUNK_SIZE);
        size_t bytes_read = file.gcount();
        
        if (bytes_read > 0) {
            if (!send_all(req.socket_fd, buffer, bytes_read)) {
                std::cerr << "Error sending file" << std::endl;
                file.close();
                return;
            }
        }
    }
    
    file.close();
    std::cout << "  File transmission complete" << std::endl;
}

void serve_put(const Request& req, const std::string& file_dir) {
    // Validate filename before constructing the path.
    if (!is_valid_filename(req.filename)) {
        send_error(req.socket_fd, "invalid filename");
        return;
    }

    // Build the destination path inside the configured file directory.
    std::string filepath = file_dir + "/" + req.filename;

    std::cout << "  PUT request: "
              << req.bytes_to_transfer
              << " bytes incoming"
              << std::endl;

    std::cout << "  Saving to: "
              << filepath
              << std::endl;

    // Open with truncation so PUT replaces an existing file.
    std::ofstream file(
        filepath,
        std::ios::binary | std::ios::trunc
    );

    if (!file.is_open()) {
        std::cerr << "Cannot create file: "
                  << filepath
                  << std::endl;

        send_error(req.socket_fd, "cannot create file");
        return;
    }

    const size_t CHUNK_SIZE = 8192;
    std::vector<char> buffer(CHUNK_SIZE);

    size_t total_received = 0;

    while (total_received < req.bytes_to_transfer) {
        size_t remaining =
            req.bytes_to_transfer - total_received;

        size_t to_receive =
            std::min(CHUNK_SIZE, remaining);

        ssize_t received = recv_data_with_timeout(
            req.socket_fd,
            buffer.data(),
            to_receive,
            5000
        );

        if (received < 0) {
            std::cerr << "Error receiving PUT data"
                      << std::endl;

            file.close();
            send_error(req.socket_fd, "receive error");
            return;
        }

        if (received == 0) {
            std::cerr << "Client closed connection before "
                      << "all PUT data was received"
                      << std::endl;

            file.close();
            send_error(req.socket_fd, "incomplete upload");
            return;
        }

        file.write(
            buffer.data(),
            static_cast<std::streamsize>(received)
        );

        if (!file.good()) {
            std::cerr << "Error writing file: "
                      << filepath
                      << std::endl;

            file.close();
            send_error(req.socket_fd, "file write error");
            return;
        }

        total_received += static_cast<size_t>(received);
    }

    file.close();

    std::cout << "  PUT complete: "
              << total_received
              << " bytes received"
              << std::endl;

    send_response(
        req.socket_fd,
        static_cast<int>(total_received)
    );
}


