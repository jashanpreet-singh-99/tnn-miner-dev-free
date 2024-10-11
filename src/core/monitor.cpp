#include "monitor.hpp"
#include "index.html.bin"

inline std::string to_string(beast::string_view str) {
    return {str.begin(), str.end()};
}

std::string_view html_content(reinterpret_cast<const char*>(html_data), html_data_len);

void handle_request(
    beast::string_view doc_root,
    bhttp::request<bhttp::string_body> req,
    bhttp::response<bhttp::string_body>& res
) {
    std::filesystem::path requested_file(to_string(doc_root) + to_string(req.target())); // doc_root + req.target();
    if (std::filesystem::exists(requested_file) && std::filesystem::is_directory(requested_file)) {
        std::ifstream file(requested_file.c_str());
        std::stringstream buffer;
        buffer << file.rdbuf();
        res.result(bhttp::status::ok);
        // res.body() = buffer.str();
        res.body() = html_content;
        res.set(bhttp::field::content_type, "text/html");
        res.prepare_payload();
    } else {
        res.result(bhttp::status::not_found);
        res.body() = "File not found!";
        res.prepare_payload();
    }
}

void start_server(std::string doc_root, unsigned short port) {
    try {
        bnet::io_context ioc(1);

        btcp::acceptor acceptor{ioc, {btcp::v4(), port}};

        while (true) {
            btcp::socket socket{ioc};
            acceptor.accept(socket);
            std::cout << "Monitoring is now served on http://localhost:" << port << std::endl;

            // HTTP request parser
            beast::flat_buffer buffer;
            bhttp::request<bhttp::string_body> req;
            bhttp::read(socket, buffer, req);

            // Respose
            bhttp::response<bhttp::string_body> res{bhttp::status::ok, req.version()};
            res.set(bhttp::field::server, "Beast");
            res.keep_alive(req.keep_alive());

            handle_request(doc_root, req, res);

            bhttp::write(socket, res);
        }
    } catch (std::exception& e) {
        std::cerr << "Error: Monitor server : " << e.what() << std::endl;
    }
}

void serve_monitor_framework(const std::string& dist_directory, unsigned short port) {
    boost::thread server_thread([=]() {
        start_server(dist_directory, port);
    });
    server_thread.detach();
}

void m_signal_handler(int signal_v) {
    std::cout << "Shutting down monitoring server......" <<std::endl;
    std::exit(signal_v);
}