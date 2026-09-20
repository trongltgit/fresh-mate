#pragma once
#include <string>
#include <nlohmann/json.hpp>

class ApiClient {
public:
    void setApiKey(const std::string& key) { m_apiKey = key; }
    bool hasKey() const { return !m_apiKey.empty(); }

    // Returns reply text or error message (starts with "ERROR:")
    std::string sendChat(const std::string& pantryContext,
                         const nlohmann::json& history,
                         const std::string& userMessage);

    // Returns JSON array string of recipes, or "ERROR:..."
    std::string generateRecipes(const std::string& pantryContext);

    // Analyze a food photo (base64) → suggested expiry + dish ideas
    // Returns JSON string: { "suggestedExpiry": "YYYY-MM-DD", "notes": "...", "dishes": [...] }
    std::string analyzeFoodPhoto(const std::string& base64Jpeg,
                                 const std::string& pantryContext);

private:
    std::string m_apiKey;
    std::string postToGroq(const nlohmann::json& body);
};
