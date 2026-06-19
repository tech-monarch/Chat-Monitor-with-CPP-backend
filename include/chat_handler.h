#pragma once

#include <vector>
#include <mutex>
#include "models.h"
#include "gemini_client.h"
#include "stats_manager.h"

class ChatHandler {
public:
    ChatHandler(GeminiClient& gemini_client, StatsManager& stats_manager);

    ChatResponse          processMessage(const std::string& prompt);
    std::vector<Message>  getHistory() const;
    void                  clearHistory();

private:
    GeminiClient&        gemini_client_;
    StatsManager&        stats_manager_;
    std::vector<Message> history_;
    mutable std::mutex   history_mutex_;

    static std::string currentTimestamp();
};
