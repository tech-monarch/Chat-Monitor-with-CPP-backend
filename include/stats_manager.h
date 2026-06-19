#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include "models.h"

class StatsManager {
public:
    StatsManager();

    void        recordRequest(long long response_time_ms, int chars_processed);
    ServerStats getStats() const;

private:
    std::atomic<int>       total_requests_;
    std::atomic<long long> total_response_time_ms_;
    std::atomic<long long> total_chars_processed_;

    std::chrono::steady_clock::time_point start_time_;
    mutable std::mutex                    mutex_;
};
