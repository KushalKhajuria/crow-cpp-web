#include <crow.h>
#include <fstream>
#include <sstream>
#include <cctype>

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

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

        // simple “analysis”
        int n = (int)text.size();
        int vowels = 0;
        for (char c : text) {
            char x = (char)std::tolower((unsigned char)c);
            if (x=='a'||x=='e'||x=='i'||x=='o'||x=='u') vowels++;
        }

        crow::json::wvalue out;
        out["original"] = text;
        out["length"] = n;
        out["vowels"] = vowels;
        return crow::response(out);
    });


    app.port(18080).run();
}
