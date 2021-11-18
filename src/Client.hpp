#pragma once
#include "Socket.hpp"
#include <future>
#include <optional>
#include <variant>
#include <vector>

namespace netlib {

    using namespace std::chrono_literals;

    class client {
    protected:
        std::optional<netlib::Socket> _socket;
        addrinfo* _endpoint_addr = nullptr;
        std::pair<std::error_condition, std::chrono::milliseconds>  wait_for_operation(socket_t sock, OperationClass op_class, std::chrono::milliseconds timeout);
    public:
        client();
        client(netlib::Socket sock, addrinfo* endpoint);
        virtual ~client();
        std::error_condition connect(const std::string& host,
                                     const std::variant<std::string, uint16_t>& service,
                                     AddressFamily address_family,
                                     AddressProtocol address_protocol,
                                     std::chrono::milliseconds);

        std::future<std::error_condition> connect_async(const std::string& host,
                                                        const std::variant<std::string, uint16_t>& service,
                                                        AddressFamily address_family,
                                                        AddressProtocol address_protocol,
                                                        std::chrono::milliseconds timeout = 1000ms);

        std::future<std::pair<std::size_t, std::error_condition>> send_async(const std::vector<uint8_t> &data, std::optional<std::chrono::milliseconds> timeout);
        std::pair<std::size_t, std::error_condition> send(const std::vector<uint8_t> &data, std::optional<std::chrono::milliseconds> timeout);

        std::future<std::pair<std::vector<uint8_t>, std::error_condition>> recv_async(std::size_t byte_count, std::optional<std::chrono::milliseconds> timeout);
        std::pair<std::vector<uint8_t>, std::error_condition> recv(std::size_t byte_count, std::optional<std::chrono::milliseconds> timeout);

        std::error_condition disconnect();
        bool is_connected();
        [[nodiscard]] std::optional<netlib::Socket> get_socket() const;
        std::optional<std::string> get_ip_addr();
    };
}// namespace netlib