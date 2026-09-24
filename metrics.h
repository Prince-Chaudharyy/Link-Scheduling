#ifndef METRICS_H
#define METRICS_H

#include "queue.h"
#include <string>

bool write_metrics_header(const std::string& filename);

bool append_metric(
    const std::string& filename,
    const Request& request
);

#endif
