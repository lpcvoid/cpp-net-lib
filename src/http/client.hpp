#include "../extern/cpp-uri-parser/URI.hpp"
#include "http.hpp"
#pragma once

namespace netlib::http {

using namespace std::chrono_literals;

class http_client {
private:
    netlib::thread_pool _thread_pool;
    netlib::client _client;
public:

    inline http_client() {}

    inline std::pair<std::optional<netlib::http::http_response>, std::error_condition> get(const std::string& url) {
        auto uri = URI(url);
        if (uri.get_result() != URI::URIParsingResult::success) {
            return std::make_pair(std::nullopt, std::errc::bad_address);
        }
        if (!_client.is_connected()) {
            uint16_t port = 80;
            if (uri.get_protocol().has_value()) {
                std::string_view protocol_str = uri.get_protocol().value();
                if (protocol_str != "http"){
                    // no support for TLS so far...
                    // also, only http and https shall ever be expected
                    return std::make_pair(std::nullopt, std::errc::protocol_not_supported);
                }
            }
            auto res = _client.connect(std::string(uri.get_host().value()), port, AddressFamily::IPv4, AddressProtocol::TCP);
            if (res) {
                return std::make_pair(std::nullopt, res);
            }
        }

        std::string query = (uri.get_query().has_value() ? std::string(uri.get_query().value()) : "/");
        std::string http_get = "GET " + query + " HTTP/1.1\r\nHost:" + std::string(uri.get_host().value()) + "\r\n\r\n";
        const std::vector<uint8_t> get_data(http_get.begin(), http_get.end());
        auto send_res = _client.send(get_data);
        if (send_res.first != get_data.size()) {
            return std::make_pair(std::nullopt, send_res.second);
        }
        std::vector<uint8_t> data_buffer;
        std::chrono::milliseconds time_spent = std::chrono::milliseconds(0);
        const std::chrono::milliseconds TICK_TIME = std::chrono::milliseconds(50);
        while (true) {
            auto recv_res = _client.recv(0, TICK_TIME);
            time_spent += TICK_TIME;
            if (!recv_res.first.empty()) {
                data_buffer.insert(data_buffer.begin(), recv_res.first.begin(), recv_res.first.end());
            }
            if ((recv_res.second == std::errc::timed_out) && (!data_buffer.empty())) {
                netlib::http::http_response response;
                std::string raw_response (data_buffer.begin(), data_buffer.end());
                std::error_condition parse_resp = response.from_raw_response(raw_response);
                if (!parse_resp) {
                    return {response, {}};
                } else {
                    return {std::nullopt, parse_resp};
                }
            }
            if (time_spent > DEFAULT_TIMEOUT) {
                return {std::nullopt, std::errc::timed_out};
            }
        }
    }

    inline std::future<std::pair<std::optional<netlib::http::http_response>, std::error_condition>> get_async(const std::string& url) {
        return _thread_pool.add_task(
            [&](std::string url) {
                return this->get(url);
            },
            url);
    }




};

}
