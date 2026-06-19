#include "gemini_client.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────

GeminiClient::GeminiClient(const std::string& api_key,
                           const std::string& model_name)
    : api_key_(api_key)
    , model_name_(model_name)
    , base_url_("https://generativelanguage.googleapis.com/v1beta/models/")
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

GeminiClient::~GeminiClient()
{
    curl_global_cleanup();
}

// ─── CURL write callback ──────────────────────────────────────────────────────

size_t GeminiClient::writeCallback(void* contents, size_t size, size_t nmemb,
                                   std::string* output)
{
    size_t total = size * nmemb;
    output->append(static_cast<char*>(contents), total);
    return total;
}

// ─── Build Gemini request body ────────────────────────────────────────────────

std::string GeminiClient::buildRequestBody(const std::string&         prompt,
                                           const std::vector<Message>& history) const
{
    json contents = json::array();

    // Inject conversation history (user / model alternation required by Gemini)
    for (const auto& msg : history) {
        json part;
        part["role"]  = (msg.role == "assistant") ? "model" : "user";
        part["parts"] = json::array({{{"text", msg.content}}});
        contents.push_back(part);
    }

    // Add the new user turn
    json current;
    current["role"]  = "user";
    current["parts"] = json::array({{{"text", prompt}}});
    contents.push_back(current);

    json body;
    body["contents"] = contents;
    body["generationConfig"] = {
        {"temperature",     1.0},
        {"topP",            0.95},
        {"maxOutputTokens", 8192}
    };

    return body.dump();
}

// ─── Parse Gemini response ────────────────────────────────────────────────────

ChatResponse GeminiClient::parseResponse(const std::string& json_str,
                                         long long          response_time_ms,
                                         int                prompt_len) const
{
    try {
        auto j = json::parse(json_str);

        // API-level error
        if (j.contains("error")) {
            std::string msg = j["error"].value("message", "Unknown Gemini error");
            int         code = j["error"].value("code", 0);
            return ChatResponse{false, "", "Gemini API error " +
                                std::to_string(code) + ": " + msg, {}};
        }

        // Safety / blocked
        if (!j.contains("candidates") || j["candidates"].empty()) {
            return ChatResponse{false, "", "Gemini returned no candidates (prompt may have been blocked)", {}};
        }

        auto& candidate = j["candidates"][0];

        // Check finish reason
        std::string finish_reason = candidate.value("finishReason", "STOP");
        if (finish_reason == "SAFETY") {
            return ChatResponse{false, "", "Response blocked by Gemini safety filters", {}};
        }

        std::string response_text =
            candidate["content"]["parts"][0]["text"].get<std::string>();

        int response_len = static_cast<int>(response_text.size());
        int total_chars  = prompt_len + response_len;

        // Timestamp
        auto        now    = std::chrono::system_clock::now();
        std::time_t now_t  = std::chrono::system_clock::to_time_t(now);
        std::tm     now_tm = *std::localtime(&now_t);
        std::ostringstream oss;
        oss << std::put_time(&now_tm, "%Y-%m-%dT%H:%M:%S");

        ChatMetrics metrics{
            model_name_,
            prompt_len,
            response_len,
            total_chars,
            response_time_ms,
            oss.str()
        };

        return ChatResponse{true, response_text, "", metrics};

    } catch (const json::exception& e) {
        return ChatResponse{false, "",
            std::string("JSON parse error: ") + e.what(), {}};
    } catch (const std::exception& e) {
        return ChatResponse{false, "",
            std::string("Unexpected error: ") + e.what(), {}};
    }
}

// ─── Public: sendMessage ──────────────────────────────────────────────────────

ChatResponse GeminiClient::sendMessage(const std::string&         prompt,
                                       const std::vector<Message>& history)
{
    std::string url = base_url_ + model_name_ +
                      ":generateContent?key=" + api_key_;
    std::string request_body  = buildRequestBody(prompt, history);
    std::string response_body;

    CURL* curl = curl_easy_init();
    if (!curl) {
        return ChatResponse{false, "", "Failed to initialise libcurl handle", {}};
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                     static_cast<long>(request_body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    auto      t0  = std::chrono::high_resolution_clock::now();
    CURLcode  res = curl_easy_perform(curl);
    auto      t1  = std::chrono::high_resolution_clock::now();

    long long elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               t1 - t0).count();

    // Read HTTP status
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return ChatResponse{false, "",
            std::string("Network error: ") + curl_easy_strerror(res), {}};
    }

    if (http_code == 401 || http_code == 403) {
        return ChatResponse{false, "",
            "Authentication failed – check your GEMINI_API_KEY", {}};
    }

    return parseResponse(response_body, elapsed_ms,
                         static_cast<int>(prompt.size()));
}
