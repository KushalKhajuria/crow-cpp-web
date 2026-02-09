#pragma once
#include <string>
#include <optional>
#include <sqlite3.h>

struct AuthResult {
    bool ok;
    std::string message;
};

AuthResult init_auth(sqlite3* db);

// returns username if logged in
std::optional<std::string> require_user(sqlite3* db, const std::string& cookie_header);

AuthResult register_user(sqlite3* db, const std::string& username, const std::string& password);
std::optional<std::string> login_user(sqlite3* db, const std::string& username, const std::string& password);

// deletes session if exists
void logout_user(sqlite3* db, const std::string& cookie_header);
