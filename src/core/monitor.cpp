#include "monitor.hpp"
#include "index.html.bin"

// html_data and html_data_len comes from binary so ignore
std::string_view html_content(reinterpret_cast<const char*>(html_data), html_data_len);

void restart_mining_threads() {
    stopMining = true;
    std::cout << "Miner restart requested....." << std::endl;
}

void handle_request(
    bhttp::request<bhttp::string_body> req,
    bhttp::response<bhttp::string_body>& res
) {
    // std::filesystem::path requested_file(to_string(doc_root) + to_string(req.target())); // doc_root + req.target();
    // if (std::filesystem::exists(requested_file) && std::filesystem::is_directory(requested_file)) {
    //     std::ifstream file(requested_file.c_str());
    //     std::stringstream buffer;
    //     buffer << file.rdbuf();
    //     res.result(bhttp::status::ok);
    //     // res.body() = buffer.str();
    //     res.body() = html_content;
    //     res.set(bhttp::field::content_type, "text/html");
    //     res.prepare_payload();
    // } else {
    //     res.result(bhttp::status::not_found);
    //     res.body() = "File not found!";
    //     res.prepare_payload();
    // }
    if (req.method() == bhttp::verb::post && req.target() == "/restart") {
        res.result(bhttp::status::ok);
        res.body() = "Restarting Miner....";
        res.prepare_payload();

        restart_mining_threads();
        return;
    }

    res.result(bhttp::status::ok);
    res.body() = html_content;
    res.set(bhttp::field::content_type, "text/html");
    res.prepare_payload();
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