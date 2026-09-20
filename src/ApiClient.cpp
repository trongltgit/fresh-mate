#include "ApiClient.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>

static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t total = size * nmemb;
    s->append(static_cast<char*>(contents), total);
    return total;
}

std::string ApiClient::postToGroq(const nlohmann::json& body) {
    if (m_apiKey.empty()) {
        return "ERROR: GROQ_API_KEY is not set. Set the environment variable and restart the server.";
    }

    CURL* curl = curl_easy_init();
    if (!curl) return "ERROR: Failed to init curl";

    std::string response;
    std::string bodyStr = body.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth = "Authorization: Bearer " + m_apiKey;
    headers = curl_slist_append(headers, auth.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.groq.com/openai/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 90L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return std::string("ERROR: curl failed: ") + curl_easy_strerror(res);
    }
    if (httpCode >= 400) {
        return "ERROR: Groq API returned " + std::to_string(httpCode) + " — " + response;
    }
    return response;
}

static std::string extractContent(const std::string& raw) {
    if (raw.rfind("ERROR:", 0) == 0) return raw;
    try {
        auto doc = nlohmann::json::parse(raw);
        if (doc.contains("choices") && doc["choices"].is_array() && !doc["choices"].empty()) {
            auto& msg = doc["choices"][0]["message"];
            if (msg.contains("content")) return msg["content"].get<std::string>();
        }
        return "ERROR: unexpected Groq response format";
    } catch (...) {
        return "ERROR: failed to parse Groq response";
    }
}

std::string ApiClient::sendChat(const std::string& pantryContext,
                                const nlohmann::json& history,
                                const std::string& userMessage) {
    std::string systemPrompt =
        "You are FreshMate, a helpful kitchen assistant focused on reducing food waste.\n"
        "You know the user's current pantry contents and help them cook, plan, and use items before they expire.\n\n"
        + pantryContext + "\n\n"
        "Answer concisely, practically and in a friendly tone. "
        "Reply in the same language the user uses.";

    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({{"role", "system"}, {"content", systemPrompt}});

    if (history.is_array()) {
        for (auto& m : history) {
            messages.push_back(m);
        }
    }
    messages.push_back({{"role", "user"}, {"content", userMessage}});

    nlohmann::json body = {
        {"model", "llama-3.3-70b-versatile"},
        {"max_tokens", 1024},
        {"messages", messages}
    };

    return extractContent(postToGroq(body));
}

std::string ApiClient::generateRecipes(const std::string& pantryContext) {
    std::string prompt =
        "You are a professional chef and food waste expert.\n\n"
        + pantryContext + "\n\n"
        "Generate 3 creative dish ideas that:\n"
        "1. Prioritise items expiring soonest\n"
        "2. Combine multiple pantry ingredients cleverly\n"
        "3. Are practical and delicious\n\n"
        "Respond ONLY with a valid JSON array — no markdown, no preamble:\n"
        "[\n"
        "  {\n"
        "    \"title\": \"Dish Name\",\n"
        "    \"icon\": \"single emoji\",\n"
        "    \"time\": \"25 mins\",\n"
        "    \"difficulty\": \"Easy\",\n"
        "    \"tags\": [\"Quick\", \"Healthy\"],\n"
        "    \"ingredients\": [\"Item 1 — how much\"],\n"
        "    \"steps\": [\"Step 1\", \"Step 2\"],\n"
        "    \"tip\": \"One helpful tip\",\n"
        "    \"pairings\": [\"Wine or drink suggestion\"]\n"
        "  }\n"
        "]";

    nlohmann::json body = {
        {"model", "llama-3.3-70b-versatile"},
        {"max_tokens", 2048},
        {"messages", nlohmann::json::array({
            {{"role", "user"}, {"content", prompt}}
        })}
    };

    std::string text = extractContent(postToGroq(body));
    if (text.rfind("ERROR:", 0) == 0) return text;

    // Extract pure JSON array if present
    auto start = text.find('[');
    auto end   = text.rfind(']');
    if (start != std::string::npos && end != std::string::npos && end > start) {
        return text.substr(start, end - start + 1);
    }
    return text;
}

std::string ApiClient::analyzeFoodPhoto(const std::string& base64Jpeg,
                                        const std::string& pantryContext) {
    // Groq vision models (update model name if Groq releases newer ones)
    std::string prompt =
        "You are a food freshness expert. Look at this food photo.\n"
        "1) Estimate a realistic expiry / best-before date (YYYY-MM-DD).\n"
        "2) Briefly explain why (color, texture, packaging if visible).\n"
        "3) Suggest 2 simple dishes that use this item soon, considering the current pantry:\n"
        + pantryContext + "\n\n"
        "Respond ONLY with valid JSON, no markdown:\n"
        "{\n"
        "  \"suggestedExpiry\": \"YYYY-MM-DD\",\n"
        "  \"notes\": \"short reason\",\n"
        "  \"dishes\": [\n"
        "    {\"title\": \"Dish name\", \"icon\": \"emoji\", \"why\": \"why this dish\"}\n"
        "  ]\n"
        "}";

    nlohmann::json content = nlohmann::json::array({
        {{"type", "text"}, {"text", prompt}},
        {{"type", "image_url"}, {"image_url", {
            {"url", "data:image/jpeg;base64," + base64Jpeg}
        }}}
    });

    nlohmann::json body = {
        {"model", "llama-3.2-11b-vision-preview"},
        {"max_tokens", 1024},
        {"messages", nlohmann::json::array({
            {{"role", "user"}, {"content", content}}
        })}
    };

    std::string text = extractContent(postToGroq(body));
    if (text.rfind("ERROR:", 0) == 0) return text;

    // Try to extract JSON object
    auto start = text.find('{');
    auto end   = text.rfind('}');
    if (start != std::string::npos && end != std::string::npos && end > start) {
        return text.substr(start, end - start + 1);
    }
    return text;
}
