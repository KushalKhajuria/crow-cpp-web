#include <crow.h>
#include <fstream>
#include <sstream>
#include <cctype>
#include <vector>
#include <mutex>
#include <ctime>
#include "number_reverser.h"

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

    CROW_ROUTE(app, "/")([]{
        crow::response res;
        res.code = 200;
        res.set_header("Content-Type", "text/html");
        res.write(readFile("../public/index.html"));
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
    ([](const crow::request& req){
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

    app.port(18080).run();
}
