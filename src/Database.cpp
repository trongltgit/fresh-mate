#include "Database.hpp"
#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>

Database::Database(const std::string& path) : m_path(path) {}

Database::~Database() {
    if (m_db) sqlite3_close(m_db);
}

bool Database::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (sqlite3_open(m_path.c_str(), &m_db) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        return false;
    }

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS products (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL,
            category    TEXT,
            quantity    TEXT,
            expiry_date TEXT NOT NULL,
            icon        TEXT,
            photo_path  TEXT,
            date_added  TEXT NOT NULL
        );
    )";
    return exec(sql);
}

bool Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        m_lastError = err ? err : "unknown";
        sqlite3_free(err);
        return false;
    }
    return true;
}

ProductItem Database::rowToItem(sqlite3_stmt* stmt) const {
    ProductItem p;
    p.id         = sqlite3_column_int(stmt, 0);
    p.name       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    p.category   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2) ? sqlite3_column_text(stmt, 2) : reinterpret_cast<const unsigned char*>(""));
    p.quantity   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3) ? sqlite3_column_text(stmt, 3) : reinterpret_cast<const unsigned char*>(""));
    p.expiryDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    p.icon       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5) ? sqlite3_column_text(stmt, 5) : reinterpret_cast<const unsigned char*>("🛒"));
    p.photoPath  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6) ? sqlite3_column_text(stmt, 6) : reinterpret_cast<const unsigned char*>(""));
    p.dateAdded  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    return p;
}

std::vector<ProductItem> Database::getAll() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ProductItem> items;
    const char* sql = "SELECT id,name,category,quantity,expiry_date,icon,photo_path,date_added FROM products ORDER BY expiry_date ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        return items;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        items.push_back(rowToItem(stmt));
    }
    sqlite3_finalize(stmt);
    return items;
}

std::optional<ProductItem> Database::getById(int id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const char* sql = "SELECT id,name,category,quantity,expiry_date,icon,photo_path,date_added FROM products WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;
    sqlite3_bind_int(stmt, 1, id);
    std::optional<ProductItem> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = rowToItem(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

int Database::add(const ProductItem& item) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // date_added = today if empty
    std::string dateAdded = item.dateAdded;
    if (dateAdded.empty()) {
        auto t = std::time(nullptr);
        std::tm* tm = std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y-%m-%d");
        dateAdded = oss.str();
    }

    const char* sql = R"(
        INSERT INTO products (name, category, quantity, expiry_date, icon, photo_path, date_added)
        VALUES (?,?,?,?,?,?,?);
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, item.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, item.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, item.quantity.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, item.expiryDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, item.icon.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, item.photoPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, dateAdded.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        m_lastError = sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        return -1;
    }
    int newId = static_cast<int>(sqlite3_last_insert_rowid(m_db));
    sqlite3_finalize(stmt);
    return newId;
}

bool Database::remove(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const char* sql = "DELETE FROM products WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) m_lastError = sqlite3_errmsg(m_db);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::update(const ProductItem& item) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const char* sql = R"(
        UPDATE products SET name=?, category=?, quantity=?, expiry_date=?, icon=?, photo_path=?
        WHERE id=?;
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, item.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, item.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, item.quantity.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, item.expiryDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, item.icon.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, item.photoPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, item.id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) m_lastError = sqlite3_errmsg(m_db);
    sqlite3_finalize(stmt);
    return ok;
}

std::string Database::pantryContextForAI() const {
    auto items = getAll();
    if (items.empty()) return "The pantry is currently empty.";

    std::ostringstream oss;
    oss << "Current pantry contents (" << items.size() << " items):\n";
    for (const auto& p : items) {
        oss << "- " << p.icon << " " << p.name
            << " (" << p.quantity << ") — " << p.expiryLabel()
            << " [category: " << p.category << "]\n";
    }
    return oss.str();
}

int Database::countByStatus(const std::string& status) const {
    auto items = getAll();
    int c = 0;
    for (const auto& p : items) {
        if (p.expiryStatus() == status) ++c;
    }
    return c;
}