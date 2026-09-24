#include "metrics.h"

#include <fstream>
#include <string>

static const char* operation_to_string(Operation op)
{
    switch (op) {
        case Operation::GET:
            return "GET";

        case Operation::PUT:
            return "PUT";

        case Operation::HEALTH:
            return "HEALTH";
    }

    return "UNKNOWN";
}

void record_completed_request(
    std::vector<Request>& completed_requests,
    std::mutex& metrics_mutex,
    const Request& request)
{
    std::lock_guard<std::mutex> lock(metrics_mutex);
    completed_requests.push_back(request);
}

bool write_metrics(
    const std::string& filename,
    const std::vector<Request>& requests)
{
    std::ofstream file(filename);

    if (!file.is_open()) {
        return false;
    }

    file << "request_id,op,filename,bytes,rounds,forfeited_bytes,"
            "arrival_ns,start_ns,finish_ns\n";

    for (const Request& request : requests) {
        // HEALTH requests are not included in the metrics CSV.
        if (request.op == Operation::HEALTH) {
            continue;
        }

        file << request.request_id << ","
             << operation_to_string(request.op) << ","
             << "\"" << request.filename << "\"" << ","
             << request.total_bytes << ","
             << request.rounds << ","
             << request.forfeited_bytes << ","
             << request.arrival_ns << ","
             << request.start_ns << ","
             << request.finish_ns
             << "\n";
    }

    return file.good();
}
