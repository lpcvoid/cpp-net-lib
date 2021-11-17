#pragma once

#include "Socket.hpp"
#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace netlib {

    using client_id_t = uint32_t;
    using callback_connect_t = std::function<std::vector<uint8_t>(client_id_t)>;
    using callback_recv_t = std::function<std::vector<uint8_t>(client_id_t, std::vector<uint8_t>)>;
    using callback_error_t = std::function<void(client_id_t, std::error_condition)>;

    class server {
    private:
        std::optional<netlib::Socket> _listener_sock;
        uint32_t _accept_queue_size = 10;
        std::vector<netlib::Socket> _clients;
        std::mutex _mutex;
        std::atomic<bool> _server_active = false;
        callback_connect_t _cb_onconnect{};
        callback_recv_t _cb_on_recv{};
        callback_error_t _cb_on_error{};
        std::thread _accept_thread;
        std::thread _processor_thread;
        void processing_func();
        void accept_func();
      public:
        server();
        std::error_condition create(const std::string& bind_host,
                                    const std::variant<std::string,uint16_t>& service,
                                    AddressFamily address_family,
                                    AddressProtocol address_protocol);
        void register_callback_on_connect(callback_connect_t onconnect) {_cb_onconnect = std::move(onconnect);};
        void register_callback_on_recv(callback_recv_t onrecv) {_cb_on_recv = std::move(onrecv);};
        void register_callback_on_error(callback_error_t onerror) {_cb_on_error = std::move(onerror);};

        std::size_t get_client_count();
    };
}

