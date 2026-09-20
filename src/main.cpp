#include "httplib.h"
#include "Database.hpp"
#include "ApiClient.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>

using json = nlohmann::json;
using namespace httplib;

json itemToJson(const ProductItem& p) {
    return {
        {"id",                p.id},
        {"name",              p.name},
        {"category",          p.category},
        {"quantity",          p.quantity},
        {"expiryDate",        p.expiryDate},
        {"icon",              p.icon},
        {"photoPath",         p.photoPath},
        {"dateAdded",         p.dateAdded},
        {"daysUntilExpiry",   p.daysUntilExpiry()},
        {"expiryStatus",      p.expiryStatus()},
        {"expiryLabel",       p.expiryLabel()},
        {"reminderFrequency", p.reminderFrequency()},
        {"reminderLabel",     p.reminderLabel()}
    };
}

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main() {
    const char* portEnv = std::getenv("PORT");
    int port = portEnv ? std::atoi(portEnv) : 8080;

    const char* apiKey = std::getenv("GROQ_API_KEY");
    const char* dbPath = std::getenv("DB_PATH");
    std::string dbFile = dbPath ? dbPath : "freshmate.db";

    Database db(dbFile);
    if (!db.initialize()) {
        std::cerr << "FATAL: cannot open DB: " << db.lastError() << std::endl;
        return 1;
    }

    ApiClient api;
    if (apiKey) api.setApiKey(apiKey);
    else std::cerr << "WARNING: GROQ_API_KEY not set — AI features disabled\n";

    Server svr;

    // ── Static files ──
    svr.Get("/", [](const Request&, Response& res) {
        auto html = readFile("public/index.html");
        if (html.empty()) {
            res.status = 404;
            res.set_content("index.html not found", "text/plain");
            return;
        }
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get("/app.js", [](const Request&, Response& res) {
        auto js = readFile("public/app.js");
        res.set_content(js.empty() ? "// missing" : js, "application/javascript");
    });

    svr.Get("/style.css", [](const Request&, Response& res) {
        auto css = readFile("public/style.css");
        res.set_content(css.empty() ? "/* missing */" : css, "text/css");
    });

    // ── API: list products ──
    svr.Get("/api/products", [&db](const Request&, Response& res) {
        auto items = db.getAll();
        json arr = json::array();
        for (auto& p : items) arr.push_back(itemToJson(p));
        res.set_content(arr.dump(), "application/json");
    });

    // ── API: summary ──
    svr.Get("/api/summary", [&db](const Request&, Response& res) {
        json j = {
            {"total",   static_cast<int>(db.getAll().size())},
            {"fresh",   db.countByStatus("ok")},
            {"warning", db.countByStatus("warn")},
            {"danger",  db.countByStatus("danger")}
        };
        res.set_content(j.dump(), "application/json");
    });

    // ── API: smart reminders ──
    // Returns items grouped by reminder frequency
    // daily / every_2_days / weekly
    svr.Get("/api/reminders", [&db](const Request&, Response& res) {
        auto items = db.getAll();
        json daily = json::array();
        json every2 = json::array();
        json weekly = json::array();

        for (auto& p : items) {
            auto freq = p.reminderFrequency();
            auto j = itemToJson(p);
            if (freq == "daily")        daily.push_back(j);
            else if (freq == "every_2_days") every2.push_back(j);
            else if (freq == "weekly")  weekly.push_back(j);
        }

        json out = {
            {"daily", daily},
            {"every_2_days", every2},
            {"weekly", weekly},
            {"message", "Reminders are based on how soon items expire: "
                        "≤2 days → daily, 3–7 days → every 2 days, 8–30 days → weekly."}
        };
        res.set_content(out.dump(), "application/json");
    });

    // ── API: add product ──
    svr.Post("/api/products", [&db](const Request& req, Response& res) {
        try {
            auto body = json::parse(req.body);
            ProductItem p;
            p.name       = body.value("name", "");
            p.category   = body.value("category", "Other");
            p.quantity   = body.value("quantity", "1");
            p.expiryDate = body.value("expiryDate", "");
            p.icon       = body.value("icon", "🛒");
            p.photoPath  = body.value("photoPath", "");

            if (p.name.empty() || p.expiryDate.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"name and expiryDate required"})", "application/json");
                return;
            }

            int id = db.add(p);
            if (id < 0) {
                res.status = 500;
                res.set_content(R"({"error":"db insert failed"})", "application/json");
                return;
            }
            p.id = id;
            res.status = 201;
            res.set_content(itemToJson(p).dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(std::string(R"({"error":")") + e.what() + "\"}", "application/json");
        }
    });

    // ── API: delete ──
    svr.Delete(R"(/api/products/(\d+))", [&db](const Request& req, Response& res) {
        int id = std::stoi(req.matches[1]);
        if (!db.remove(id)) {
            res.status = 404;
            res.set_content(R"({"error":"not found"})", "application/json");
            return;
        }
        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── API: chat ──
    svr.Post("/api/chat", [&db, &api](const Request& req, Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string msg = body.value("message", "");
            json history = body.value("history", json::array());

            if (msg.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"message required"})", "application/json");
                return;
            }

            std::string context = db.pantryContextForAI();
            std::string reply = api.sendChat(context, history, msg);

            if (reply.rfind("ERROR:", 0) == 0) {
                res.status = 502;
                json err = {{"error", reply.substr(6)}};
                res.set_content(err.dump(), "application/json");
                return;
            }
            json out = {{"reply", reply}};
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(std::string(R"({"error":")") + e.what() + "\"}", "application/json");
        }
    });

    // ── API: recipes ──
    svr.Post("/api/recipes", [&db, &api](const Request&, Response& res) {
        std::string context = db.pantryContextForAI();
        std::string raw = api.generateRecipes(context);

        if (raw.rfind("ERROR:", 0) == 0) {
            res.status = 502;
            json err = {{"error", raw.substr(6)}};
            res.set_content(err.dump(), "application/json");
            return;
        }

        try {
            auto arr = json::parse(raw);
            res.set_content(arr.dump(), "application/json");
        } catch (...) {
            json out = {{"raw", raw}};
            res.set_content(out.dump(), "application/json");
        }
    });

    // Health
    svr.Get("/health", [](const Request&, Response& res) {
        res.set_content("OK", "text/plain");
    });

    std::cout << "FreshMate Web server starting on 0.0.0.0:" << port << std::endl;
    std::cout << "Open http://localhost:" << port << std::endl;
    svr.listen("0.0.0.0", port);
    return 0;
}
