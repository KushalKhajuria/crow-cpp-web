#include "auth.h"
#include <sodium.h>
#include <ctime>
#include <sstream>
#include <iomanip>

static std::string get_cookie_value(const std::string& cookie, const std::string& key) {
    // very small cookie parser: looks for "key=value"
    // cookie header format: "a=1; sid=XYZ; b=2"
    std::string needle = key + "=";
    size_t pos = cookie.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    size_t end = cookie.find(';', pos);
    std::string val = cookie.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    while (!val.empty() && val.front() == ' ') val.erase(val.begin());
    while (!val.empty() && val.back() == ' ') val.pop_back();
    return val;
}

static std::string random_sid_hex() {
    unsigned char buf[16];
    randombytes_buf(buf, sizeof(buf));
    std::ostringstream oss;
    for (unsigned char c : buf) oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    return oss.str();
}

static bool exec_sql(sqlite3* db, const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK;
}

AuthResult init_auth(sqlite3* db) {
    if (sodium_init() < 0) return {false, "libsodium init failed"};

    const char* users_sql =
        "CREATE TABLE IF NOT EXISTS users ("
        " username TEXT PRIMARY KEY,"
        " pw_hash  TEXT NOT NULL,"
        " created_at INTEGER NOT NULL"
        ");";

    const char* sessions_sql =
        "CREATE TABLE IF NOT EXISTS sessions ("
        " sid TEXT PRIMARY KEY,"
        " username TEXT NOT NULL,"
        " created_at INTEGER NOT NULL,"
        " expires_at INTEGER NOT NULL"
        ");";

    if (!exec_sql(db, users_sql)) return {false, "failed to create users table"};
    if (!exec_sql(db, sessions_sql)) return {false, "failed to create sessions table"};
    return {true, "ok"};
}

AuthResult register_user(sqlite3* db, const std::string& username, const std::string& password) {
    if (username.empty() || password.size() < 6) {
        return {false, "username required and password must be >= 6 chars"};
    }

    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hash, password.c_str(), password.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        return {false, "password hashing failed"};
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO users(username, pw_hash, created_at) VALUES(?,?,?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {false, "db prepare failed"};
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)std::time(nullptr));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return {false, "username already exists (or db error)"};
    return {true, "registered"};
}

std::optional<std::string> login_user(sqlite3* db, const std::string& username, const std::string& password) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT pw_hash FROM users WHERE username=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); return std::nullopt; }

    const char* hash = (const char*)sqlite3_column_text(stmt, 0);
    if (!hash) { sqlite3_finalize(stmt); return std::nullopt; }

    bool ok = (crypto_pwhash_str_verify(hash, password.c_str(), password.size()) == 0);
    sqlite3_finalize(stmt);
    if (!ok) return std::nullopt;

    // create session (7 days)
    std::string sid = random_sid_hex();
    std::time_t now = std::time(nullptr);
    std::time_t exp = now + 7 * 24 * 60 * 60;

    sqlite3_stmt* ins = nullptr;
    const char* ins_sql = "INSERT INTO sessions(sid, username, created_at, expires_at) VALUES(?,?,?,?);";
    if (sqlite3_prepare_v2(db, ins_sql, -1, &ins, nullptr) != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(ins, 1, sid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(ins, 3, (sqlite3_int64)now);
    sqlite3_bind_int64(ins, 4, (sqlite3_int64)exp);

    rc = sqlite3_step(ins);
    sqlite3_finalize(ins);
    if (rc != SQLITE_DONE) return std::nullopt;

    return sid;
}

std::optional<std::string> require_user(sqlite3* db, const std::string& cookie_header) {
    std::string sid = get_cookie_value(cookie_header, "sid");
    if (sid.empty()) return std::nullopt;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT username, expires_at FROM sessions WHERE sid=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(stmt, 1, sid.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); return std::nullopt; }

    const unsigned char* user_raw = sqlite3_column_text(stmt, 0);
    sqlite3_int64 exp = sqlite3_column_int64(stmt, 1);

    // ✅ copy before finalize
    std::string username = user_raw ? reinterpret_cast<const char*>(user_raw) : "";

    sqlite3_finalize(stmt);

    if (username.empty()) return std::nullopt;

    if ((sqlite3_int64)std::time(nullptr) > exp) {
        // expired -> delete it
        sqlite3_stmt* del = nullptr;
        if (sqlite3_prepare_v2(db, "DELETE FROM sessions WHERE sid=?;", -1, &del, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(del, 1, sid.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(del);
            sqlite3_finalize(del);
        }
        return std::nullopt;
    }

    return username;
}

void logout_user(sqlite3* db, const std::string& cookie_header) {
    std::string sid = get_cookie_value(cookie_header, "sid");
    if (sid.empty()) return;

    sqlite3_stmt* del = nullptr;
    if (sqlite3_prepare_v2(db, "DELETE FROM sessions WHERE sid=?;", -1, &del, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(del, 1, sid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(del);
    sqlite3_finalize(del);
}
