#pragma once
#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>

struct ProductItem {
    int         id          = 0;
    std::string name;
    std::string category;
    std::string quantity;
    std::string expiryDate;   // YYYY-MM-DD
    std::string icon;
    std::string photoPath;
    std::string dateAdded;    // YYYY-MM-DD

    // Days until expiry (negative = expired)
    int daysUntilExpiry() const {
        std::tm tm = {};
        std::istringstream ss(expiryDate);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail()) return 9999;

        auto exp = std::mktime(&tm);
        auto now = std::time(nullptr);
        std::tm* nowTm = std::localtime(&now);
        nowTm->tm_hour = 0; nowTm->tm_min = 0; nowTm->tm_sec = 0;
        auto today = std::mktime(nowTm);

        return static_cast<int>(std::difftime(exp, today) / 86400.0);
    }

    // "ok" | "warn" | "danger"
    std::string expiryStatus() const {
        int d = daysUntilExpiry();
        if (d < 0)  return "danger";
        if (d <= 3) return "warn";
        return "ok";
    }

    std::string expiryLabel() const {
        int d = daysUntilExpiry();
        if (d < 0)  return "Expired " + std::to_string(-d) + " day(s) ago";
        if (d == 0) return "Expires today";
        if (d == 1) return "Expires tomorrow";
        return "Expires in " + std::to_string(d) + " days";
    }

    // Smart reminder frequency based on how close expiry is
    // "daily" | "every_2_days" | "weekly" | "none"
    std::string reminderFrequency() const {
        int d = daysUntilExpiry();
        if (d < 0)   return "daily";       // already expired → remind daily
        if (d <= 2)  return "daily";       // 0–2 days left → daily
        if (d <= 7)  return "every_2_days";// 3–7 days left → every 2 days
        if (d <= 30) return "weekly";      // 8–30 days → weekly
        return "none";                     // > 30 days → no active reminder
    }

    std::string reminderLabel() const {
        auto f = reminderFrequency();
        if (f == "daily")        return "Remind daily";
        if (f == "every_2_days") return "Remind every 2 days";
        if (f == "weekly")       return "Remind weekly";
        return "No active reminder";
    }
};
