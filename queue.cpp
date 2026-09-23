#include "queue.h"

bool RequestQueue::push(Request request)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (closed_) {
            return false;
        }

        queue_.push_back(std::move(request));
    }

    cv_.notify_one();
    return true;
}

bool RequestQueue::wait_pop(Request& request)
{
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this]() {
        return !queue_.empty() || closed_;
    });

    if (queue_.empty()) {
        return false;
    }

    request = std::move(queue_.front());
    queue_.pop_front();

    return true;
}

bool RequestQueue::try_pop(Request& request)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.empty()) {
        return false;
    }

    request = std::move(queue_.front());
    queue_.pop_front();

    return true;
}

bool RequestQueue::requeue(Request request)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (closed_) {
            return false;
        }

        queue_.push_back(std::move(request));
    }

    cv_.notify_one();
    return true;
}

std::size_t RequestQueue::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool RequestQueue::empty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

void RequestQueue::close()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
    }

    cv_.notify_all();
}

bool RequestQueue::is_closed() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
}