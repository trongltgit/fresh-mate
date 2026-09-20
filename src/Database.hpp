#pragma once
#include "ProductItem.hpp"
#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include <sqlite3.h>

class Database {
public:
    explicit Database(const std::string& path = "freshmate.db");
    ~Database();

    bool initialize();
    std::string lastError() const { return m_lastError; }

    // CRUD
    std::vector<ProductItem> getAll() const;
    std::optional<ProductItem> getById(int id) const;
    int  add(const ProductItem& item);          // returns new id
    bool remove(int id);
    bool update(const ProductItem& item);

    // Helpers for AI context
    std::string pantryContextForAI() const;

    int countByStatus(const std::string& status) const; // "ok"|"warn"|"danger"

private:
    sqlite3*     m_db = nullptr;
    std::string  m_path;
    mutable std::string m_lastError;
    mutable std::mutex  m_mutex;

    bool exec(const std::string& sql);
    ProductItem rowToItem(sqlite3_stmt* stmt) const;
};