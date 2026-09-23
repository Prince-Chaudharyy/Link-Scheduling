#include <iostream>
#include <cstring>
#include <thread>
#include <csignal>
#include <sys/socket.h>

#include "config.h"
#include "socket.h"
#include "protocol.h"
#include "request.h"
#include "file_io.h"
#include "server.h"

volatile std::sig_atomic_t shutdown_flag = 0;
int global_listen_socket = -1;

void handle_signal(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        shutdown_flag = 1;

        if (global_listen_socket >= 0) {
            shutdown(global_listen_socket, SHUT_RDWR);
        }
    }
}

void acceptor_thread(int listen_socket, const std::string& file_dir) {
    int request_id = 0;
    
    while (!shutdown_flag) {
        int client_socket = accept_connection(listen_socket);
        if (client_socket < 0) continue;
        
        // Read header with 1 second timeout (A5)
        std::string header = read_header_line(client_socket, 1000);
        if (header.empty()) {
            send_error(client_socket, "timeout or invalid request");
            close_socket(client_socket);
            continue;
        }
        
        // Parse the header
        try {
            Request req = parse_request_header(header, client_socket);
            req.request_id = ++request_id;
            
            std::cout << "Request " << req.request_id << ": " << req.op 
                      << " " << req.filename << std::endl;
           
	     // HEALTH does not have a filename.
            if (req.op == "HEALTH") {
               send_response(client_socket, 0);
               std::cout << "  HEALTH check" << std::endl;
            }
            else {
                // GET and PUT require a valid filename.
                if (!is_valid_filename(req.filename)) {
                    send_error(client_socket, "invalid filename");
                    close_socket(client_socket);
                    continue;
                }

                if (req.op == "GET") {
                    serve_get(req, file_dir);
                }
                else if (req.op == "PUT") {
                   serve_put(req, file_dir);
                }
            }
            
            close_socket(client_socket);
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            send_error(client_socket, e.what());
            close_socket(client_socket);
        }
    }
}

int main(int argc, char* argv[]) {
   
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    // Default values
    std::string config_path = "config.json";
    std::string sched_policy;
    std::string file_dir;
    std::string metrics_out = "metrics.csv";
    size_t quantum = 0;
    int packetization = 1;
    
    // Parse command line
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--sched") == 0 && i + 1 < argc) {
            sched_policy = argv[++i];
        } else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            file_dir = argv[++i];
        } else if (strcmp(argv[i], "--quantum") == 0 && i + 1 < argc) {
            quantum = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--p") == 0 && i + 1 < argc) {
            packetization = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--metrics-out") == 0 && i + 1 < argc) {
            metrics_out = argv[++i];
        }
    }
    
    // Validate required flags
    if (sched_policy.empty()) {
        std::cerr << "error: missing required flag '--sched'" << std::endl;
        return 1;
    }
    
    if (file_dir.empty()) {
        std::cerr << "error: missing required flag '--file'" << std::endl;
        return 1;
    }
    
    // Load config
    Config cfg;
    try {
        cfg = load_config(config_path);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    
    // Validate quantum for RR/DRR
    if ((sched_policy == "rr" || sched_policy == "drr") && quantum == 0) {
        std::cerr << "error: --quantum required for " << sched_policy << std::endl;
        return 1;
    }
    
    if ((sched_policy == "fcfs" || sched_policy == "sjf") && quantum != 0) {
        std::cerr << "error: --quantum not allowed for " << sched_policy << std::endl;
        return 1;
    }
    
    std::cout << "Starting server..." << std::endl;
    std::cout << "Policy: " << sched_policy << std::endl;
    std::cout << "File directory: " << file_dir << std::endl;
    std::cout << "Threads: " << cfg.server.server_threads << std::endl;
    std::cout << "Metrics output: " << metrics_out << std::endl;
    std::cout << "Packetization: " << packetization << " lines" << std::endl;
    
    // Create listening socket
    int listen_socket = create_listening_socket(cfg.server.ip, cfg.server.port);
 
    if (listen_socket < 0) {
         std::cerr << "Failed to create listening socket" << std::endl;
         return 1;
    }

    global_listen_socket = listen_socket;

    std::cout << "Server started! Listening on " << cfg.server.ip << ":" 
          << cfg.server.port << std::endl;
    
    // Start acceptor thread
    std::cout << "Waiting for connections (Ctrl+C to stop)..." << std::endl;
    std::thread acceptor(acceptor_thread, listen_socket, file_dir);
    
    // Wait for Ctrl+C
    acceptor.join();
    
    close_socket(listen_socket);
    return 0;
}

