#include "monitor.hpp"
#include "binary_config.h"

// Helper function to convert string_view to string
inline std::string to_string(boost::core::basic_string_view<char> str) {
    return {str.data(), str.size()};
}

// Helper function to determine content type based on the file extension
beast::string_view get_content_type(const std::string& path) {
    const std::unordered_map<std::string, beast::string_view> mime_types = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".svg", "image/svg+xml"}
    };
    
    std::string extension;
    if (path == "/") {
        extension = ".html"; // special case for root
    } else {
        size_t dot_pos = path.find_last_of('.');
        if (dot_pos != std::string::npos) {
            extension = path.substr(dot_pos);
        } else {
            extension = "";
        }
    }

    return mime_types.count(extension) ? mime_types.at(extension) : "application/octet-stream";
}

void restart_mining_threads() {
    stopMining = true;
    std::cout << "Miner restart requested....." << std::endl;
}

void handle_request(
    bhttp::request<bhttp::string_body> req,
    bhttp::response<bhttp::string_body>& res
) {
    std::string requested_path = to_string(req.target());
    bhttp::verb method = req.method();
    std::cout << "Request:" << method << ": " << requested_path << std::endl;

    if (method == bhttp::verb::get) {
        // GET
        if (requested_path == "/" ||
            requested_path == "/Dash" ||
            requested_path == "/Payment" ||
            requested_path == "/History" ||
            requested_path == "/Settings") 
        {
            requested_path = "/index.html";
        }

        // Resource file request
        auto it = file_map.find(requested_path);
        if (it != file_map.end()) {
            const auto& [data, length] = it->second;
            // std::cout << "Data: " << data << std::endl;
            // std::cout << "Length: " << length << std::endl;

            res.result(bhttp::status::ok);
            res.body() = boost::core::basic_string_view<char>(reinterpret_cast<const char*>(data), length);
            res.set(bhttp::field::content_type, get_content_type(requested_path));
            res.prepare_payload();
            return;
        }

        try {
            res.result(bhttp::status::ok);
            res.body() = "Not implemented: data fetch only.";
            res.set(bhttp::field::content_type, "text/plain");
        } catch (const std::exception& e) {
            res.result(bhttp::status::not_found);
            res.body() = "Requested file not found: " + requested_path;
        }

        res.prepare_payload();
    } else if (method == bhttp::verb::post) {
        // POST
        if (requested_path == "/restart") {
            res.result(bhttp::status::ok);
            res.body() = "Restarting Miner....";
            res.prepare_payload();

            restart_mining_threads();
            return;
        }
    }
}

void start_server(unsigned short port) {
    try {
        bnet::io_context ioc(1);

        btcp::acceptor acceptor{ioc, {btcp::v4(), port}};
        std::cout << "Monitoring is now served on http://localhost:" << port << std::endl;

        while (true) {
            btcp::socket socket{ioc};
            acceptor.accept(socket);

            // HTTP request parser
            beast::flat_buffer buffer;
            bhttp::request<bhttp::string_body> req;
            bhttp::read(socket, buffer, req);

            // Respose
            bhttp::response<bhttp::string_body> res{bhttp::status::ok, req.version()};
            res.set(bhttp::field::server, "Beast");
            res.keep_alive(req.keep_alive());

            handle_request(req, res);

            bhttp::write(socket, res);
        }
    } catch (std::exception& e) {
        std::cerr << "Error: Monitor server : " << e.what() << std::endl;
    }
}

void serve_monitor_framework(unsigned short port) {
    boost::thread server_thread([=]() {
        start_server(port);
    });
    server_thread.detach();
}

void m_signal_handler(int signal_v) {
    std::cout << "Shutting down monitoring server......" <<std::endl;
    std::exit(signal_v);
}