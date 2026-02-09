#include <crow.h>
#include <fstream>
#include <sstream>
#include <cctype>
#include <vector>
#include <mutex>
#include <ctime>
#include "number_reverser.h"
#include "othello/othello.h"
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

int main() {
    crow::SimpleApp app;
    Othello game;

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

    CROW_ROUTE(app, "/api/run-game").methods(crow::HTTPMethod::Post)
    ([&]{
        game.runGame();
        crow::json::wvalue out;
        out["ok"] = true;
        out["message"] = "Game started.";
        return crow::response(out);
    });

    CROW_ROUTE(app, "/api/othello/state").methods(crow::HTTPMethod::Get)
    ([&]{
        const auto& board = game.getBoard().getBoard();
        crow::json::wvalue out;
        out["ok"] = true;
        out["turn"] = game.getCurrentSide();
        out["board"] = crow::json::wvalue::list();
        for (size_t r = 0; r < board.size(); r++) {
            out["board"][r] = crow::json::wvalue::list();
            for (size_t c = 0; c < board[r].size(); c++) {
                out["board"][r][c] = board[r][c];
            }
        }
        return crow::response(out);
    });

    CROW_ROUTE(app, "/api/othello/move").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("row") || !body.has("col")) {
            return crow::response(400, "Expected JSON: {\"row\":0,\"col\":0}");
        }

        int row = body["row"].i();
        int col = body["col"].i();

        bool ok = game.placePiece(row, col);
        const auto& board = game.getBoard().getBoard();

        crow::json::wvalue out;
        out["ok"] = ok;
        out["turn"] = game.getCurrentSide();
        out["board"] = crow::json::wvalue::list();
        for (size_t r = 0; r < board.size(); r++) {
            out["board"][r] = crow::json::wvalue::list();
            for (size_t c = 0; c < board[r].size(); c++) {
                out["board"][r][c] = board[r][c];
            }
        }

        return crow::response(ok ? 200 : 400, out);
    });

    app.port(18080).run();
}
