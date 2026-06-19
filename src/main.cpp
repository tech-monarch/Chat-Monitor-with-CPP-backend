#include "crow.h"
#include "crow/middlewares/cors.h"
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include "chat_handler.h"
#include "gemini_client.h"
#include "stats_manager.h"

using json = nlohmann::json;

// ─── .env loader ─────────────────────────────────────────────────────────────
static void loadDotEnv(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key   = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Strip surrounding quotes
        if (value.size() >= 2
            && ((value.front() == '"' && value.back() == '"')
                || (value.front() == '\'' && value.back() == '\'')))
        {
            value = value.substr(1, value.size() - 2);
        }

        // Only set if not already in environment
        if (std::getenv(key.c_str()) == nullptr) {
        #ifdef _WIN32
            _putenv_s(key.c_str(), value.c_str());
        #else
            setenv(key.c_str(), value.c_str(), 0);
        #endif
        }
    }
}

static std::string env(const std::string& key,
                       const std::string& fallback = "")
{
    const char* v = std::getenv(key.c_str());
    return v ? std::string(v) : fallback;
}

// ─── JSON helper ─────────────────────────────────────────────────────────────
static crow::response jsonResponse(int code, const json& body)
{
    crow::response res(code, body.dump());
    res.set_header("Content-Type", "application/json");
    return res;
}

// ─── main ─────────────────────────────────────────────────────────────────────
int main()
{
    loadDotEnv(".env");

    // ── Validate required env vars ──────────────────────────────────────────
    std::string api_key    = env("GEMINI_API_KEY");
    std::string model_name = env("GEMINI_MODEL", "gemini-2.5-flash");
    int         port       = std::stoi(env("PORT", "8080"));

    if (api_key.empty()) {
        std::cerr << "[FATAL] GEMINI_API_KEY is not set.\n"
                  << "        Copy .env.example to .env and fill in your key.\n";
        return 1;
    }

    // ── Component initialisation ────────────────────────────────────────────
    GeminiClient gemini_client(api_key, model_name);
    StatsManager stats_manager;
    ChatHandler  chat_handler(gemini_client, stats_manager);

    std::cout << "[INFO]  Gemini Chat Monitor\n"
              << "[INFO]  Model  : " << model_name << "\n"
              << "[INFO]  Port   : " << port       << "\n"
              << "[INFO]  Key    : " << api_key.substr(0, 8) << "...\n";

    // ── Crow app with CORS middleware ────────────────────────────────────────
    crow::App<crow::CORSHandler> app;

    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .headers("Content-Type", "Authorization")
        .methods("GET"_method, "POST"_method, "DELETE"_method, "OPTIONS"_method)
        .origin("*");

    // ── GET /api/health ──────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/health").methods("GET"_method)
    ([&]() {
        return jsonResponse(200, {
            {"status",  "ok"},
            {"service", "Gemini Chat Monitor"},
            {"version", "1.0.0"},
            {"model",   gemini_client.modelName()}
        });
    });

    // ── GET /api/stats ───────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/stats").methods("GET"_method)
    ([&]() {
        ServerStats s = stats_manager.getStats();
        return jsonResponse(200, {
            {"total_requests",        s.total_requests},
            {"average_response_time", s.average_response_time},
            {"total_tokens_estimate", s.total_tokens_estimate},
            {"uptime_seconds",        s.uptime_seconds}
        });
    });

    // ── POST /api/chat ───────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/chat").methods("POST"_method)
    ([&](const crow::request& req) {
        // ── Parse body ──────────────────────────────────────────────────────
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            return jsonResponse(400, {{"error", "Invalid JSON body"}});
        }

        if (!body.contains("prompt") || !body["prompt"].is_string()) {
            return jsonResponse(400, {{"error", "Missing string field 'prompt'"}});
        }

        std::string prompt = body["prompt"].get<std::string>();

        if (prompt.empty()) {
            return jsonResponse(400, {{"error", "Prompt must not be empty"}});
        }
        if (prompt.size() > 10000) {
            return jsonResponse(400, {{"error", "Prompt exceeds 10 000-character limit"}});
        }

        // ── Process ─────────────────────────────────────────────────────────
        ChatResponse resp = chat_handler.processMessage(prompt);

        if (!resp.success) {
            // Distinguish auth errors (4xx) from server errors (5xx)
            int code = 500;
            if (resp.error.find("Authentication") != std::string::npos
                || resp.error.find("API key")      != std::string::npos)
            {
                code = 401;
            }
            return jsonResponse(code, {{"error", resp.error}});
        }

        return jsonResponse(200, {
            {"message", resp.message},
            {"metrics", {
                {"model",            resp.metrics.model},
                {"prompt_length",    resp.metrics.prompt_length},
                {"response_length",  resp.metrics.response_length},
                {"total_characters", resp.metrics.total_characters},
                {"response_time_ms", resp.metrics.response_time_ms},
                {"timestamp",        resp.metrics.timestamp}
            }}
        });
    });

    // ── GET /api/history ─────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/history").methods("GET"_method)
    ([&]() {
        auto history = chat_handler.getHistory();
        json messages = json::array();
        for (const auto& m : history) {
            messages.push_back({
                {"role",      m.role},
                {"content",   m.content},
                {"timestamp", m.timestamp}
            });
        }
        return jsonResponse(200, {{"messages", messages}});
    });

    // ── DELETE /api/history ──────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/history").methods("DELETE"_method)
    ([&]() {
        chat_handler.clearHistory();
        return jsonResponse(200, {{"message", "Chat history cleared"}});
    });

    // ── Launch ───────────────────────────────────────────────────────────────
    app.port(port)
       .multithreaded()
       .run();

    return 0;
}
