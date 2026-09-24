#ifndef METRICS_H
#define METRICS_H

#include "queue.h"

#include <mutex>
#include <string>
#include <vector>

bool write_metrics(
    const std::string& filename,
    const std::vector<Request>& requests
);

void record_completed_request(
    std::vector<Request>& completed_requests,
    std::mutex& metrics_mutex,
    const Request& request
);

#endif
