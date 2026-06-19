#pragma once

#include <string>
#include <vector>

// ─── Data Models ─────────────────────────────────────────────────────────────

struct Message {
    std::string role;       // "user" | "assistant"
    std::string content;
    std::string timestamp;
};

struct ChatMetrics {
    std::string model;
    int         prompt_length    = 0;
    int         response_length  = 0;
    int         total_characters = 0;
    long long   response_time_ms = 0;
    std::string timestamp;
};

struct ChatResponse {
    bool        success = false;
    std::string message;
    std::string error;
    ChatMetrics metrics;
};

struct ServerStats {
    int         total_requests        = 0;
    double      average_response_time = 0.0;
    long long   total_tokens_estimate = 0;
    long long   uptime_seconds        = 0;
};
