#include "server.h"

#include "file_io.h"
#include "protocol.h"
#include "socket.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

bool serve_get(
    Request& req,
    const std::string& file_dir,
    std::uint64_t budget,
    std::uint64_t& bytes_processed)
{
    bytes_processed = 0;

    if (!is_valid_filename(req.filename)) {
        send_error(req.client_fd, "invalid filename");
        return false;
    }

    const std::string filepath = file_dir + "/" + req.filename;

    if (!file_exists(filepath)) {
        std::cerr << "File not found: " << filepath << std::endl;
        send_error(req.client_fd, "file not found");
        return false;
    }

    const std::uint64_t file_size =
        static_cast<std::uint64_t>(get_file_size(filepath));

    req.total_bytes = file_size;

    // Send the GET response header only once.
    if (!req.response_sent) {
        send_response(req.client_fd, file_size);
        req.response_sent = true;

        std::cout
            << "  GET response: "
            << file_size
            << " bytes"
            << std::endl;
    }

    // Nothing left to send.
    if (req.offset >= file_size || budget == 0) {
        return true;
    }

    const std::uint64_t remaining = file_size - req.offset;
    const std::uint64_t to_send =
        std::min(budget, remaining);

    std::ifstream file(filepath, std::ios::binary);

    if (!file.is_open()) {
        send_error(req.client_fd, "cannot open file");
        return false;
    }

    file.seekg(
        static_cast<std::streamoff>(req.offset),
        std::ios::beg
    );

    if (!file.good()) {
        file.close();
        send_error(req.client_fd, "file seek error");
        return false;
    }

    const std::size_t CHUNK_SIZE = 8192;
    std::vector<char> buffer(
        std::min<std::uint64_t>(CHUNK_SIZE, to_send)
    );

    std::uint64_t remaining_round = to_send;

    while (remaining_round > 0) {
        const std::size_t chunk =
            static_cast<std::size_t>(
                std::min<std::uint64_t>(
                    buffer.size(),
                    remaining_round
                )
            );

        file.read(
            buffer.data(),
            static_cast<std::streamsize>(chunk)
        );

        const std::streamsize bytes_read = file.gcount();

        if (bytes_read <= 0) {
            file.close();
            send_error(req.client_fd, "file read error");
            return false;
        }

        if (!send_all(
                req.client_fd,
                buffer.data(),
                static_cast<std::size_t>(bytes_read))) {

            file.close();
            std::cerr << "Error sending file" << std::endl;
            return false;
        }

        bytes_processed +=
            static_cast<std::uint64_t>(bytes_read);

        remaining_round -=
            static_cast<std::uint64_t>(bytes_read);
    }

    file.close();

    std::cout
        << "  Sent "
        << bytes_processed
        << " bytes this round"
        << " (offset "
        << req.offset + bytes_processed
        << "/"
        << file_size
        << ")"
        << std::endl;

    return true;
}


bool serve_put(
    Request& req,
    const std::string& file_dir,
    std::uint64_t budget,
    std::uint64_t& bytes_processed)
{
    bytes_processed = 0;

    if (!is_valid_filename(req.filename)) {
        send_error(req.client_fd, "invalid filename");
        return false;
    }

    const std::string filepath = file_dir + "/" + req.filename;

    std::cout
        << "  PUT request: "
        << req.total_bytes
        << " bytes incoming"
        << std::endl;

    std::cout
        << "  Saving to: "
        << filepath
        << std::endl;

    // A zero-byte PUT is valid.
    if (req.total_bytes == 0) {
        return true;
    }

    if (req.offset == 0) {
        // First round: create/truncate the destination.
        std::ofstream create_file(
            filepath,
            std::ios::binary | std::ios::trunc
        );

        if (!create_file.is_open()) {
            send_error(req.client_fd, "cannot create file");
            return false;
        }

        create_file.close();
    }

    std::fstream file(
        filepath,
        std::ios::binary |
        std::ios::in |
        std::ios::out
    );

    if (!file.is_open()) {
        send_error(req.client_fd, "cannot open file");
        return false;
    }

    file.seekp(
        static_cast<std::streamoff>(req.offset),
        std::ios::beg
    );

    if (!file.good()) {
        file.close();
        send_error(req.client_fd, "file seek error");
        return false;
    }

    const std::uint64_t remaining =
        req.total_bytes - req.offset;

    const std::uint64_t to_receive =
        std::min(budget, remaining);

    const std::size_t CHUNK_SIZE = 8192;

    std::vector<char> buffer(
        std::min<std::uint64_t>(CHUNK_SIZE, to_receive)
    );

    std::uint64_t remaining_round = to_receive;

    while (remaining_round > 0) {
        const std::size_t chunk =
            static_cast<std::size_t>(
                std::min<std::uint64_t>(
                    buffer.size(),
                    remaining_round
                )
            );

        const ssize_t received =
            recv_data_with_timeout(
                req.client_fd,
                buffer.data(),
                chunk,
                5000
            );

        if (received < 0) {
            file.close();
            send_error(req.client_fd, "receive error");
            return false;
        }

        if (received == 0) {
            file.close();
            send_error(req.client_fd, "incomplete upload");
            return false;
        }

        file.write(
            buffer.data(),
            static_cast<std::streamsize>(received)
        );

        if (!file.good()) {
            file.close();
            send_error(req.client_fd, "file write error");
            return false;
        }

        bytes_processed +=
            static_cast<std::uint64_t>(received);

        remaining_round -=
            static_cast<std::uint64_t>(received);
    }

    file.close();

    std::cout
        << "  Received "
        << bytes_processed
        << " bytes this round"
        << " (offset "
        << req.offset + bytes_processed
        << "/"
        << req.total_bytes
        << ")"
        << std::endl;

    return true;
}
