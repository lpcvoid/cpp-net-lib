#pragma once
#include "endpoint_accessor.hpp"
#include "service_resolver.hpp"
#include "socket.hpp"
#include "socket_operations.hpp"
#include "thread_pool.hpp"
#include <future>
#include <iostream>
#include <optional>
#include <variant>
#include <vector>

namespace netlib {

using namespace std::chrono_literals;

class client {
private:
    static constexpr std::chrono::milliseconds DEFAULT_TIMEOUT = 1000ms;
protected:
    std::optional<netlib::socket> _socket;
    addrinfo *_endpoint_addr = nullptr;
    netlib::thread_pool _thread_pool = netlib::thread_pool::create<1,1>();
public:
    client()
    {
        netlib::socket::initialize_system();
    }
    client(netlib::socket sock, addrinfo *endpoint)
    {
        _socket = sock;
        _endpoint_addr = endpoint;
        netlib::socket::initialize_system();
    }
    virtual ~client()
    {
        disconnect();
    }
#ifdef BUILD_OPENSSL

    inline std::error_condition use_openssl(bool enable) {
        if (is_connected()) {
            //disconnect the current session
            auto dc_error = disconnect();
            if (dc_error) {
                return dc_error;
            }
        }
    }

#endif
    inline std::error_condition connect(const std::string &host, const std::variant<std::string, uint16_t> &service,
                                        AddressFamily address_family, AddressProtocol address_protocol, std::chrono::milliseconds timeout = DEFAULT_TIMEOUT)
    {
        if (is_connected()) {
            auto ec = disconnect();
            if (ec && ec != std::errc::not_connected) {
                return ec;
            }
        }
        const std::string service_string =
            std::holds_alternative<uint16_t>(service) ? std::to_string(std::get<uint16_t>(service)) : std::get<std::string>(service);
        std::pair<addrinfo *, std::error_condition> addrinfo_result =
            service_resolver::get_addrinfo(host, service_string, address_family, address_protocol, AI_ADDRCONFIG);
        if (addrinfo_result.first == nullptr) {
            return addrinfo_result.second;
        }
        std::error_condition global_connect_error{};
        for (addrinfo *res_addrinfo = addrinfo_result.first; res_addrinfo != nullptr; res_addrinfo = res_addrinfo->ai_next) {
            auto sock = netlib::socket();
            std::error_condition s_create_error =
                sock.create(res_addrinfo->ai_family, res_addrinfo->ai_socktype, res_addrinfo->ai_protocol);
            if (s_create_error) {
                freeaddrinfo(addrinfo_result.first);
                return s_create_error; // we already failed creating the socket, oof
            }
            sock.set_nonblocking(true);
            int32_t connect_result = ::connect(sock.get_raw().value(), res_addrinfo->ai_addr, res_addrinfo->ai_addrlen);
            if (connect_result == -1) {
                std::error_condition error_condition = socket_get_last_error();
                if ((error_condition != std::errc::operation_in_progress) && (error_condition != std::errc::operation_would_block)) {
                    freeaddrinfo(addrinfo_result.first);
                    return error_condition;
                }
            }
            auto connect_error = netlib::operations::wait_for_operation(sock.get_raw().value(), OperationClass::write, timeout);
            if (connect_error.first) {
                sock.close();
                global_connect_error = connect_error.first;
                continue;
            }

            _endpoint_addr = res_addrinfo;
            _socket = sock;
            break;
        }
        if (_socket) {
            return {};
        }
        return global_connect_error;
    }

    inline std::future<std::error_condition> connect_async(const std::string &host, const std::variant<std::string, uint16_t> &service,
                                                           AddressFamily address_family, AddressProtocol address_protocol,
                                                           std::chrono::milliseconds timeout = DEFAULT_TIMEOUT)
    {
        return _thread_pool.add_task(
            [&](const std::string &host, const std::variant<std::string, uint16_t> &service, netlib::AddressFamily address_family,
                netlib::AddressProtocol address_protocol, std::chrono::milliseconds timeout) {
                return this->connect(host, service, address_family, address_protocol, timeout);
            },
            host, service, address_family, address_protocol, timeout);
    }

    /*!
     * @brief Send data to an endpoint in async fashion. See `send` for parameter reference.
     * @return Returns a future with the same contents that `send` returns.
     * @extends send
     */

    inline std::future<std::pair<std::size_t, std::error_condition>> send_async(const std::vector<uint8_t> &data,
                                                                                std::chrono::milliseconds timeout = DEFAULT_TIMEOUT)
    {
        return _thread_pool.add_task(
            [&](const std::vector<uint8_t> &data, std::chrono::milliseconds timeout) {
                return this->send(data, timeout);
            },
            data, timeout);
    }


    /*!
     * @brief Send data to a server.
     *
     * @remark Assumes that a connection was established. Returns \p not_connected if not.
     *
     * @param data The data that is to be transmitted.
     *
     * @param timeout The max amount of time that this function may wait until
     * returning. The function may return sooner than \p timeout, for example if an error
     * was detected or if all bytes indicated in \p byte_count where sent.
     *
     * @return Returns a pair with the actual amount of data sent and an error.
     */

    inline std::pair<std::size_t, std::error_condition> send(const std::vector<uint8_t> &data,
                                                             std::chrono::milliseconds timeout = DEFAULT_TIMEOUT)
    {

        if (!is_connected()) {
            return {0, std::errc::not_connected};
        }

        if (timeout.count() < 0) {
            return {0, std::errc::timed_out};
        }

        auto send_result = netlib::operations::send(_socket.value(), data, timeout);

        if (send_result.second && send_result.second == std::errc::connection_aborted) {
            disconnect();
        }
        return send_result;
    }

    /*!
     * @brief Recieve data from a server in async fashion. See `recv` for parameter reference.
     * @return Returns a future with the same contents that `recv` returns.
     * @extends recv
     */

    inline std::future<std::pair<std::vector<uint8_t>, std::error_condition>> recv_async(std::size_t byte_count = 0,
                                                                                         std::chrono::milliseconds timeout = 0ms)
    {
        return _thread_pool.add_task(
            [&](std::size_t byte_count, std::chrono::milliseconds timeout) {
                return this->recv(byte_count, timeout);
            },
            byte_count, timeout);
    }

    /*!
     * @brief Receive data from a server.
     *
     * @param byte_count The maximum amount of data that is expected. The actual
     * data received may be less than this. If 0 is passed, the function will wait
     * until \p timeout has passed and return whatever it got within this time.
     *
     * @param timeout The max amount of time that this function may wait until
     * returning. If \p byte_count is 0, the function will wait at most \p timeout
     * milliseconds and return whatever has been received until then. The function
     *                may return sooner than \p timeout, for example if an error
     * was detected or if all bytes indicated in \p byte_count where received.
     *
     * @return Returns a pair with the data and an error. Data and error may both
     * be emtpy, but not at the same time. There are valid cases for both an error
     * and data to be returned - http trafic is one exapmple, where a client can
     *         be disconnected by the server after request was served.
     */
    inline std::pair<std::vector<uint8_t>, std::error_condition> recv(std::size_t byte_count = 0, std::chrono::milliseconds timeout = 0ms)
    {

        if (!is_connected()) {
            return {{}, std::errc::not_connected};
        }

        if (timeout.count() < 0) {
            return {{}, std::errc::timed_out};
        }
        auto recv_res = netlib::operations::recv(_socket.value(), byte_count, timeout);
        if (recv_res.second && recv_res.second != std::errc::timed_out) {
            disconnect();
        }
        return recv_res;
    }

    inline std::error_condition disconnect()
    {
        if (!_socket.has_value()) {
            return std::errc::not_connected;
        }
        _socket->close();
        _socket.reset();

        if (_endpoint_addr) {
            freeaddrinfo(_endpoint_addr);
        }

        return {};
    }
    inline bool is_connected()
    {
        return _socket.has_value() && _socket->is_valid();
    }
    [[nodiscard]] inline std::optional<netlib::socket> get_socket() const
    {
        return _socket;
    }
    inline std::optional<std::string> get_ip_addr()
    {
        if (!_endpoint_addr) {
            return std::nullopt;
        }
        return endpoint_accessor::ip_to_string(_endpoint_addr);
    }
};
} // namespace netlib