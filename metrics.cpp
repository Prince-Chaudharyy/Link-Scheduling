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

bool write_metrics_header(const std::string& filename)
{
    std::ofstream file(filename);

    if (!file.is_open()) {
        return false;
    }

    file << "request_id,op,filename,bytes,rounds,forfeited_bytes,"
            "arrival_ns,start_ns,finish_ns\n";

    return file.good();
}

bool append_metric(
    const std::string& filename,
    const Request& request)
{
    std::ofstream file(filename, std::ios::app);

    if (!file.is_open()) {
        return false;
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

    return file.good();
}

