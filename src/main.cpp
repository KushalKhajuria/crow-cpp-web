#include <crow.h>
#include <fstream>
#include <sstream>
#include <cctype>
#include <vector>
#include <mutex>
#include <ctime>
#include <sstream>
#include "number_reverser.h"
#include "othello/board/board.h"
#include <sqlite3.h>
#include "auth.h"

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct Submission {
    std::string text;
    int length;
    int vowels;
    std::time_t created;
};

std::vector<Submission> submissions;
std::mutex submissions_mtx;

static bool exec_sql(sqlite3* db, const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK;
}

static std::vector<std::vector<int>> initial_board() {
    std::vector<std::vector<int>> b(8, std::vector<int>(8, 0));
    b[3][3] = 1;
    b[3][4] = -1;
    b[4][3] = -1;
    b[4][4] = 1;
    return b;
}

static std::string board_to_json(const std::vector<std::vector<int>>& b) {
    std::ostringstream oss;
    oss << "[";
    for (size_t r = 0; r < b.size(); r++) {
        if (r) oss << ",";
        oss << "[";
        for (size_t c = 0; c < b[r].size(); c++) {
            if (c) oss << ",";
            oss << b[r][c];
        }
        oss << "]";
    }
    oss << "]";
    return oss.str();
}

static std::vector<std::vector<int>> board_from_json(const std::string& s) {
    std::vector<std::vector<int>> out;
    auto j = crow::json::load(s);
    if (!j || j.t() != crow::json::type::List) return out;
    out.resize(j.size());
    for (size_t r = 0; r < j.size(); r++) {
        out[r].resize(j[r].size());
        for (size_t c = 0; c < j[r].size(); c++) {
            out[r][c] = j[r][c].i();
        }
    }
    return out;
}

int main() {
    crow::SimpleApp app;

    sqlite3* db = nullptr;
    if (sqlite3_open("app.db", &db) != SQLITE_OK) {
        std::cerr << "Failed to open app.db\n";
        return 1;
    }
    auto init = init_auth(db);
    if (!init.ok) {
        std::cerr << init.message << "\n";
        return 1;
    }

    const char* games_sql =
        "CREATE TABLE IF NOT EXISTS games ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " player1 TEXT NOT NULL,"
        " player2 TEXT NOT NULL,"
        " turn INTEGER NOT NULL,"
        " pass_count INTEGER NOT NULL DEFAULT 0,"
        " draw_offer_by TEXT,"
        " board TEXT NOT NULL,"
        " status TEXT NOT NULL,"
        " created_at INTEGER NOT NULL,"
        " updated_at INTEGER NOT NULL"
        ");";
    if (!exec_sql(db, games_sql)) {
        std::cerr << "Failed to create games table\n";
        return 1;
    }
    exec_sql(db, "ALTER TABLE games ADD COLUMN pass_count INTEGER NOT NULL DEFAULT 0;");
    exec_sql(db, "ALTER TABLE games ADD COLUMN draw_offer_by TEXT;");

    CROW_ROUTE(app, "/")([]{
        std::ifstream f("public/index.html"); // <-- assumes you run ./app from project root
        if (!f) {
            return crow::response(500, "Could not open public/index.html. Check working directory.");
        }

        std::stringstream ss;
        ss << f.rdbuf();

        crow::response res;
        res.code = 200;
        res.set_header("Content-Type", "text/html; charset=UTF-8");
        res.write(ss.str());
        return res;
    });


    CROW_ROUTE(app, "/api/hello")([]{
        crow::json::wvalue x;
        x["message"] = "Hello from C++";
        return x;
    });

    CROW_ROUTE(app, "/api/analyze").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("text")) {
            return crow::response(400, "Expected JSON: {\"text\":\"...\"}");
        }

        std::string text = body["text"].s();

        int n = (int)text.size();
        int vowels = 0;
        for (char c : text) {
            char x = (char)std::tolower((unsigned char)c);
            if (x=='a'||x=='e'||x=='i'||x=='o'||x=='u') vowels++;
        }

        Submission s{ text, n, vowels, std::time(nullptr) };

        { // lock scope
            std::lock_guard<std::mutex> lock(submissions_mtx);
            submissions.push_back(std::move(s));
        }

        crow::json::wvalue out;
        out["ok"] = true;
        out["length"] = n;
        out["vowels"] = vowels;
        return crow::response(out);
    });

    CROW_ROUTE(app, "/api/reverse").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){

        auto user = require_user(db, req.get_header_value("Cookie"));
        if (!user) return crow::response(401, "Login required");

        auto body = crow::json::load(req.body);
        if (!body || !body.has("numbers")) {
            return crow::response(400, "Expected JSON: { \"numbers\": [1,2,3] }");
        }

        std::vector<int> nums;
        for (size_t i = 0; i < body["numbers"].size(); i++) {
            nums.push_back(body["numbers"][i].i());
        }

        // 🔥 call separate C++ logic
        std::vector<int> reversed = NumberReverser::reverse(nums);

        crow::json::wvalue res;
        res["original"] = crow::json::wvalue::list();
        res["reversed"] = crow::json::wvalue::list();

        for (size_t i = 0; i < nums.size(); i++) {
            res["original"][i] = nums[i];
            res["reversed"][i] = reversed[i];
        }

        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/submissions").methods(crow::HTTPMethod::Get)
    ([]{
        std::lock_guard<std::mutex> lock(submissions_mtx);

        crow::json::wvalue res;
        res["count"] = (int)submissions.size();

        // Make a JSON list/array and fill by index
        res["items"] = crow::json::wvalue::list();
        for (size_t i = 0; i < submissions.size(); i++) {
            const auto& s = submissions[i];

            crow::json::wvalue item;
            item["text"] = s.text;
            item["length"] = s.length;
            item["vowels"] = s.vowels;
            item["created"] = (long long)s.created;

            res["items"][i] = std::move(item);
        }

        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/register").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password"))
            return crow::response(400, "Expected {username,password}");

        auto r = register_user(db, body["username"].s(), body["password"].s());
        crow::json::wvalue out;
        out["ok"] = r.ok;
        out["message"] = r.message;
        return crow::response(r.ok ? 200 : 400, out);
    });

    CROW_ROUTE(app, "/api/login").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("username") || !body.has("password"))
            return crow::response(400, "Expected {username,password}");

        auto sid = login_user(db, body["username"].s(), body["password"].s());
        if (!sid) return crow::response(401, "Invalid credentials");

        crow::json::wvalue out;
        out["ok"] = true;

        crow::response res(out);
        res.set_header("Set-Cookie", "sid=" + *sid + "; HttpOnly; Path=/; SameSite=Lax");
        return res;
    });

    CROW_ROUTE(app, "/api/logout").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        std::string cookie = req.get_header_value("Cookie");
        logout_user(db, cookie);

        crow::json::wvalue out;
        out["ok"] = true;

        crow::response res(out);
        res.set_header("Set-Cookie", "sid=; Max-Age=0; Path=/; SameSite=Lax");
        return res;
    });

    CROW_ROUTE(app, "/api/me").methods(crow::HTTPMethod::Get)
    ([&](const crow::request& req){
        auto user = require_user(db, req.get_header_value("Cookie"));
        if (!user) return crow::response(401, "Not logged in");

        crow::json::wvalue out;
        out["ok"] = true;
        out["username"] = *user;
        return crow::response(out);
    });

    CROW_ROUTE(app, "/api/games/create").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto user = require_user(db, req.get_header_value("Cookie"));
        if (!user) return crow::response(401, "Login required");

        auto body = crow::json::load(req.body);
        if (!body || !body.has("opponent")) {
            return crow::response(400, "Expected JSON: {\"opponent\":\"username\"}");
        }

        std::string opponent = body["opponent"].s();
        if (opponent.empty()) return crow::response(400, "Opponent required");

        sqlite3_stmt* chk = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT username FROM users WHERE username=?;", -1, &chk, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        sqlite3_bind_text(chk, 1, opponent.c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(chk);
        sqlite3_finalize(chk);
        if (rc != SQLITE_ROW) return crow::response(404, "Opponent not found");

        sqlite3_stmt* active = nullptr;
        const char* active_sql =
            "SELECT id FROM games WHERE status='active' AND (player1=? OR player2=? OR player1=? OR player2=?);";
        if (sqlite3_prepare_v2(db, active_sql, -1, &active, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        sqlite3_bind_text(active, 1, user->c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(active, 2, user->c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(active, 3, opponent.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(active, 4, opponent.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(active);
        sqlite3_finalize(active);
        if (rc == SQLITE_ROW) return crow::response(409, "Player already in active game");

        std::string board_json = board_to_json(initial_board());
        std::time_t now = std::time(nullptr);

        sqlite3_stmt* ins = nullptr;
        const char* ins_sql =
            "INSERT INTO games(player1, player2, turn, pass_count, draw_offer_by, board, status, created_at, updated_at)"
            " VALUES(?,?,?,?,?,?,?,?,?);";
        if (sqlite3_prepare_v2(db, ins_sql, -1, &ins, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        sqlite3_bind_text(ins, 1, user->c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 2, opponent.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(ins, 3, 1);
        sqlite3_bind_int(ins, 4, 0);
        sqlite3_bind_null(ins, 5);
        sqlite3_bind_text(ins, 6, board_json.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 7, "active", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(ins, 8, (sqlite3_int64)now);
        sqlite3_bind_int64(ins, 9, (sqlite3_int64)now);
        rc = sqlite3_step(ins);
        sqlite3_finalize(ins);
        if (rc != SQLITE_DONE) return crow::response(500, "DB error");

        crow::json::wvalue out;
        out["ok"] = true;
        out["game_id"] = (int)sqlite3_last_insert_rowid(db);
        return crow::response(out);
    });

    CROW_ROUTE(app, "/api/games/<int>/state").methods(crow::HTTPMethod::Get)
    ([&](const crow::request& req, int game_id){
        auto viewer = require_user(db, req.get_header_value("Cookie"));
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT player1, player2, turn, pass_count, draw_offer_by, board, status FROM games WHERE id=?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        sqlite3_bind_int(stmt, 1, game_id);
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); return crow::response(404, "Game not found"); }

        std::string p1 = (const char*)sqlite3_column_text(stmt, 0);
        std::string p2 = (const char*)sqlite3_column_text(stmt, 1);
        int turn = sqlite3_column_int(stmt, 2);
        int pass_count = sqlite3_column_int(stmt, 3);
        const unsigned char* draw_raw = sqlite3_column_text(stmt, 4);
        std::string draw_offer_by = draw_raw ? (const char*)draw_raw : "";
        std::string board_json = (const char*)sqlite3_column_text(stmt, 5);
        std::string status = (const char*)sqlite3_column_text(stmt, 6);
        sqlite3_finalize(stmt);

        auto board = board_from_json(board_json);

        // Auto-pass if current player has no moves.
        bool did_pass = false;
        if (status == "active") {
            bool can_autopass = false;
            if (viewer && (*viewer == p1 || *viewer == p2)) {
                int viewer_side = (*viewer == p1) ? 1 : -1;
                if (viewer_side == turn) can_autopass = true;
            }
            if (can_autopass) {
                Board game_board;
                game_board.setBoard(board);
                if (!game_board.anyMoves(turn)) {
                int next_turn = (turn == 1) ? -1 : 1;
                int next_pass = pass_count + 1;
                const char* next_status = (next_pass >= 2) ? "finished" : "active";

                sqlite3_stmt* upd = nullptr;
                const char* upd_sql =
                    "UPDATE games SET turn=?, pass_count=?, draw_offer_by=NULL, status=?, updated_at=? WHERE id=?;";
                if (sqlite3_prepare_v2(db, upd_sql, -1, &upd, nullptr) == SQLITE_OK) {
                    sqlite3_bind_int(upd, 1, next_turn);
                    sqlite3_bind_int(upd, 2, next_pass);
                    sqlite3_bind_text(upd, 3, next_status, -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int64(upd, 4, (sqlite3_int64)std::time(nullptr));
                    sqlite3_bind_int(upd, 5, game_id);
                    sqlite3_step(upd);
                    sqlite3_finalize(upd);
                }

                turn = next_turn;
                pass_count = next_pass;
                status = next_status;
                draw_offer_by.clear();
                did_pass = true;
            }
            }
        }

        crow::json::wvalue out;
        out["ok"] = true;
        out["game_id"] = game_id;
        out["player1"] = p1;
        out["player2"] = p2;
        out["turn"] = turn;
        out["status"] = status;
        out["pass_count"] = pass_count;
        out["draw_offer_by"] = draw_offer_by;
        if (did_pass) out["message"] = "No valid moves. Turn passed.";
        out["board"] = crow::json::wvalue::list();
        for (size_t r = 0; r < board.size(); r++) {
            out["board"][r] = crow::json::wvalue::list();
            for (size_t c = 0; c < board[r].size(); c++) {
                out["board"][r][c] = board[r][c];
            }
        }
        return crow::response(out);
    });

    CROW_ROUTE(app, "/api/games/active").methods(crow::HTTPMethod::Get)
    ([&](const crow::request& req){
        auto user = require_user(db, req.get_header_value("Cookie"));
        if (!user) return crow::response(401, "Login required");

        sqlite3_stmt* stmt = nullptr;
        const char* sql =
            "SELECT id FROM games WHERE status='active' AND (player1=? OR player2=?) "
            "ORDER BY updated_at DESC LIMIT 1;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        sqlite3_bind_text(stmt, 1, user->c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user->c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); return crow::response(404, "No active game"); }

        int game_id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        crow::json::wvalue out;
        out["ok"] = true;
        out["game_id"] = game_id;
        return crow::response(out);
    });

    CROW_ROUTE(app, "/api/games/<int>/move").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req, int game_id){
        auto user = require_user(db, req.get_header_value("Cookie"));
        if (!user) return crow::response(401, "Login required");

        auto body = crow::json::load(req.body);
        if (!body || !body.has("row") || !body.has("col")) {
            return crow::response(400, "Expected JSON: {\"row\":0,\"col\":0}");
        }
        int row = body["row"].i();
        int col = body["col"].i();

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT player1, player2, turn, pass_count, draw_offer_by, board, status FROM games WHERE id=?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        sqlite3_bind_int(stmt, 1, game_id);
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); return crow::response(404, "Game not found"); }

        std::string p1 = (const char*)sqlite3_column_text(stmt, 0);
        std::string p2 = (const char*)sqlite3_column_text(stmt, 1);
        int turn = sqlite3_column_int(stmt, 2);
        int pass_count = sqlite3_column_int(stmt, 3);
        const unsigned char* draw_raw = sqlite3_column_text(stmt, 4);
        std::string draw_offer_by = draw_raw ? (const char*)draw_raw : "";
        std::string board_json = (const char*)sqlite3_column_text(stmt, 5);
        std::string status = (const char*)sqlite3_column_text(stmt, 6);
        sqlite3_finalize(stmt);

        if (status != "active") return crow::response(400, "Game not active");

        std::time_t now = std::time(nullptr);

        int side = 0;
        if (*user == p1) side = 1;
        else if (*user == p2) side = -1;
        else return crow::response(403, "Not a player in this game");

        if (turn != side) return crow::response(409, "Not your turn");

        auto board = board_from_json(board_json);
        if (board.size() != 8 || row < 0 || row > 7 || col < 0 || col > 7) {
            return crow::response(400, "Invalid move");
        }
        if (board[row][col] != 0) return crow::response(400, "Space occupied");

        Board game_board;
        game_board.setBoard(board);

        if (!game_board.anyMoves(side)) {
            int next_turn = (side == 1) ? -1 : 1;
            int next_pass = pass_count + 1;
            const char* next_status = (next_pass >= 2) ? "finished" : "active";

            sqlite3_stmt* upd = nullptr;
            const char* upd_sql = "UPDATE games SET turn=?, pass_count=?, draw_offer_by=NULL, status=?, updated_at=? WHERE id=?;";
            if (sqlite3_prepare_v2(db, upd_sql, -1, &upd, nullptr) != SQLITE_OK) {
                return crow::response(500, "DB error");
            }
            sqlite3_bind_int(upd, 1, next_turn);
            sqlite3_bind_int(upd, 2, next_pass);
            sqlite3_bind_text(upd, 3, next_status, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(upd, 4, (sqlite3_int64)now);
            sqlite3_bind_int(upd, 5, game_id);
            sqlite3_step(upd);
            sqlite3_finalize(upd);

            crow::json::wvalue out;
            out["ok"] = true;
            out["game_id"] = game_id;
            out["turn"] = next_turn;
            out["pass_count"] = next_pass;
            out["status"] = next_status;
            out["draw_offer_by"] = "";
            out["message"] = "No valid moves. Turn passed.";
            out["board"] = crow::json::wvalue::list();
            for (size_t r = 0; r < board.size(); r++) {
                out["board"][r] = crow::json::wvalue::list();
                for (size_t c = 0; c < board[r].size(); c++) {
                    out["board"][r][c] = board[r][c];
                }
            }
            return crow::response(out);
        }

        bool ok = game_board.addPiece(row, col, side);
        if (!ok) return crow::response(400, "Invalid move");

        board = game_board.getBoard();
        int next_turn = (side == 1) ? -1 : 1;

        std::string new_json = board_to_json(board);

        sqlite3_stmt* upd = nullptr;
        const char* upd_sql = "UPDATE games SET turn=?, pass_count=?, draw_offer_by=NULL, board=?, status=?, updated_at=? WHERE id=?;";
        if (sqlite3_prepare_v2(db, upd_sql, -1, &upd, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        int next_pass = 0;
        const char* next_status = (next_pass >= 2) ? "finished" : "active";
        sqlite3_bind_int(upd, 1, next_turn);
        sqlite3_bind_int(upd, 2, next_pass);
        sqlite3_bind_text(upd, 3, new_json.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(upd, 4, next_status, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(upd, 5, (sqlite3_int64)now);
        sqlite3_bind_int(upd, 6, game_id);
        sqlite3_step(upd);
        sqlite3_finalize(upd);

        crow::json::wvalue out;
        out["ok"] = true;
        out["game_id"] = game_id;
        out["turn"] = next_turn;
        out["pass_count"] = next_pass;
        out["status"] = next_status;
        out["draw_offer_by"] = "";
        out["board"] = crow::json::wvalue::list();
        for (size_t r = 0; r < board.size(); r++) {
            out["board"][r] = crow::json::wvalue::list();
            for (size_t c = 0; c < board[r].size(); c++) {
                out["board"][r][c] = board[r][c];
            }
        }
        return crow::response(out);
    });

    CROW_ROUTE(app, "/api/games/<int>/resign").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req, int game_id){
        auto user = require_user(db, req.get_header_value("Cookie"));
        if (!user) return crow::response(401, "Login required");

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT player1, player2, status FROM games WHERE id=?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        sqlite3_bind_int(stmt, 1, game_id);
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); return crow::response(404, "Game not found"); }

        std::string p1 = (const char*)sqlite3_column_text(stmt, 0);
        std::string p2 = (const char*)sqlite3_column_text(stmt, 1);
        std::string status = (const char*)sqlite3_column_text(stmt, 2);
        sqlite3_finalize(stmt);

        if (status != "active") return crow::response(400, "Game not active");
        if (*user != p1 && *user != p2) return crow::response(403, "Not a player in this game");

        std::string winner = (*user == p1) ? p2 : p1;

        sqlite3_stmt* upd = nullptr;
        const char* upd_sql = "UPDATE games SET status='resigned', draw_offer_by=NULL, updated_at=? WHERE id=?;";
        if (sqlite3_prepare_v2(db, upd_sql, -1, &upd, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        sqlite3_bind_int64(upd, 1, (sqlite3_int64)std::time(nullptr));
        sqlite3_bind_int(upd, 2, game_id);
        sqlite3_step(upd);
        sqlite3_finalize(upd);

        crow::json::wvalue out;
        out["ok"] = true;
        out["status"] = "resigned";
        out["winner"] = winner;
        return crow::response(out);
    });

    CROW_ROUTE(app, "/api/games/<int>/offer-draw").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req, int game_id){
        auto user = require_user(db, req.get_header_value("Cookie"));
        if (!user) return crow::response(401, "Login required");

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT player1, player2, status FROM games WHERE id=?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        sqlite3_bind_int(stmt, 1, game_id);
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); return crow::response(404, "Game not found"); }

        std::string p1 = (const char*)sqlite3_column_text(stmt, 0);
        std::string p2 = (const char*)sqlite3_column_text(stmt, 1);
        std::string status = (const char*)sqlite3_column_text(stmt, 2);
        sqlite3_finalize(stmt);

        if (status != "active") return crow::response(400, "Game not active");
        if (*user != p1 && *user != p2) return crow::response(403, "Not a player in this game");

        sqlite3_stmt* upd = nullptr;
        const char* upd_sql = "UPDATE games SET draw_offer_by=?, updated_at=? WHERE id=?;";
        if (sqlite3_prepare_v2(db, upd_sql, -1, &upd, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        sqlite3_bind_text(upd, 1, user->c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(upd, 2, (sqlite3_int64)std::time(nullptr));
        sqlite3_bind_int(upd, 3, game_id);
        sqlite3_step(upd);
        sqlite3_finalize(upd);

        crow::json::wvalue out;
        out["ok"] = true;
        out["status"] = "active";
        out["draw_offer_by"] = *user;
        return crow::response(out);
    });

    CROW_ROUTE(app, "/api/games/<int>/accept-draw").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req, int game_id){
        auto user = require_user(db, req.get_header_value("Cookie"));
        if (!user) return crow::response(401, "Login required");

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT player1, player2, status, draw_offer_by FROM games WHERE id=?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        sqlite3_bind_int(stmt, 1, game_id);
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); return crow::response(404, "Game not found"); }

        std::string p1 = (const char*)sqlite3_column_text(stmt, 0);
        std::string p2 = (const char*)sqlite3_column_text(stmt, 1);
        std::string status = (const char*)sqlite3_column_text(stmt, 2);
        const unsigned char* draw_raw = sqlite3_column_text(stmt, 3);
        std::string draw_offer_by = draw_raw ? (const char*)draw_raw : "";
        sqlite3_finalize(stmt);

        if (status != "active") return crow::response(400, "Game not active");
        if (*user != p1 && *user != p2) return crow::response(403, "Not a player in this game");
        if (draw_offer_by.empty()) return crow::response(409, "No draw offer");
        if (draw_offer_by == *user) return crow::response(409, "You cannot accept your own offer");

        sqlite3_stmt* upd = nullptr;
        const char* upd_sql = "UPDATE games SET status='draw', draw_offer_by=NULL, updated_at=? WHERE id=?;";
        if (sqlite3_prepare_v2(db, upd_sql, -1, &upd, nullptr) != SQLITE_OK) {
            return crow::response(500, "DB error");
        }
        sqlite3_bind_int64(upd, 1, (sqlite3_int64)std::time(nullptr));
        sqlite3_bind_int(upd, 2, game_id);
        sqlite3_step(upd);
        sqlite3_finalize(upd);

        crow::json::wvalue out;
        out["ok"] = true;
        out["status"] = "draw";
        return crow::response(out);
    });

    app.port(18080).run();
}
