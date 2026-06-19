#include "stats_manager.h"

StatsManager::StatsManager()
    : total_requests_(0)
    , total_response_time_ms_(0)
    , total_chars_processed_(0)
    , start_time_(std::chrono::steady_clock::now())
{}

void StatsManager::recordRequest(long long response_time_ms, int chars_processed)
{
    total_requests_.fetch_add(1, std::memory_order_relaxed);
    total_response_time_ms_.fetch_add(response_time_ms, std::memory_order_relaxed);
    total_chars_processed_.fetch_add(chars_processed, std::memory_order_relaxed);
}

ServerStats StatsManager::getStats() const
{
    auto now    = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                      now - start_time_).count();

    int       reqs       = total_requests_.load(std::memory_order_relaxed);
    long long total_time = total_response_time_ms_.load(std::memory_order_relaxed);
    long long total_ch   = total_chars_processed_.load(std::memory_order_relaxed);

    double avg_time      = (reqs > 0) ? (static_cast<double>(total_time) / reqs) : 0.0;
    long long tokens_est = total_ch / 4;   // rough estimate: ~4 chars per token

    return ServerStats{reqs, avg_time, tokens_est, uptime};
}
