#pragma once

#include <string>
#include <vector>
#include <curl/curl.h>
#include "models.h"

class GeminiClient {
public:
    explicit GeminiClient(const std::string& api_key,
                          const std::string& model_name = "gemini-2.5-flash");
    ~GeminiClient();

    // Non-copyable
    GeminiClient(const GeminiClient&)            = delete;
    GeminiClient& operator=(const GeminiClient&) = delete;

    ChatResponse sendMessage(const std::string&         prompt,
                             const std::vector<Message>& history);

    const std::string& modelName() const { return model_name_; }

private:
    std::string api_key_;
    std::string model_name_;
    std::string base_url_;

    static size_t writeCallback(void*        contents,
                                size_t       size,
                                size_t       nmemb,
                                std::string* output);

    std::string  buildRequestBody(const std::string&         prompt,
                                  const std::vector<Message>& history) const;

    ChatResponse parseResponse(const std::string& json_str,
                               long long          response_time_ms,
                               int                prompt_len) const;
};
