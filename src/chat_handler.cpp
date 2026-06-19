#include "chat_handler.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

ChatHandler::ChatHandler(GeminiClient& gemini_client,
                         StatsManager& stats_manager)
    : gemini_client_(gemini_client)
    , stats_manager_(stats_manager)
{}

// ─────────────────────────────────────────────────────────────────────────────

ChatResponse ChatHandler::processMessage(const std::string& prompt)
{
    // Snapshot current history (avoid holding lock during network call)
    std::vector<Message> history_snap;
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        history_snap = history_;
    }

    ChatResponse resp = gemini_client_.sendMessage(prompt, history_snap);

    if (resp.success) {
        stats_manager_.recordRequest(resp.metrics.response_time_ms,
                                     resp.metrics.total_characters);

        std::lock_guard<std::mutex> lock(history_mutex_);
        history_.push_back(Message{"user",      prompt,       resp.metrics.timestamp});
        history_.push_back(Message{"assistant", resp.message, resp.metrics.timestamp});
    }

    return resp;
}

std::vector<Message> ChatHandler::getHistory() const
{
    std::lock_guard<std::mutex> lock(history_mutex_);
    return history_;
}

void ChatHandler::clearHistory()
{
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.clear();
}

std::string ChatHandler::currentTimestamp()
{
    auto        now   = std::chrono::system_clock::now();
    std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    std::tm     tm    = *std::localtime(&now_t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}
