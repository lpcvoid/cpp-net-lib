//
// Created by lpcvoid on 11/11/2021.
//

#include "client.hpp"
#include "endpoint_accessor.hpp"
#include "service_resolver.hpp"
#include <cstring>

netlib::client::client() {
}

netlib::client::client(netlib::socket sock, addrinfo *endpoint) {
  _socket = sock;
  _endpoint_addr = endpoint;
}

netlib::client::~client() {
  disconnect();
}

std::pair<std::size_t, std::error_condition> netlib::client::send(const std::vector<uint8_t> &data, std::optional<std::chrono::milliseconds> timeout) {
    if (!is_connected()) {
        return {0, std::errc::not_connected};
    }

    if (timeout.has_value() && timeout->count() < 0) {
        return {0, std::errc::timed_out};
    }

    auto wait_res = wait_for_operation(_socket.value().get_raw().value(), OperationClass::write, timeout.value());
    if (wait_res.first) {
        return {0, wait_res.first};
    }
    int32_t send_res = ::send(_socket.value().get_raw().value(), data.data(), data.size(), 0);
    if (send_res >= 0) {
        if (send_res == data.size()) {
            return {send_res, {}};
        }
        return {send_res, std::errc::message_size};
    }
    return {0, socket_get_last_error()};
}

std::pair<std::vector<uint8_t>, std::error_condition> netlib::client::recv(std::size_t byte_count, std::optional<std::chrono::milliseconds> timeout) {
    if (!is_connected()) {
        return {{}, std::errc::not_connected};
    }

    if (timeout.has_value() && timeout->count() < 0) {
        return {{}, std::errc::timed_out};
    }

    auto wait_res = wait_for_operation(_socket.value().get_raw().value(), OperationClass::read, timeout.value());
    if (wait_res.first) {
        return {{}, wait_res.first};
    }

    std::vector<uint8_t> data(byte_count, 0);
    int32_t recv_res = ::recv(_socket.value().get_raw().value(), data.data(), byte_count, 0);
    if (recv_res > 0) {
        data.resize(recv_res);
        return {data, {}};
    } else if (recv_res == 0){
      return {{}, std::errc::connection_aborted};
    }
    return {{}, socket_get_last_error()};
}



std::error_condition netlib::client::connect(const std::string &host,
                                             const std::variant<std::string,uint16_t>& service,
                                             netlib::AddressFamily address_family,
                                             netlib::AddressProtocol address_protocol,
                                             std::chrono::milliseconds timeout) {
    if (is_connected()) {
        auto ec = disconnect();
        if (ec && ec != std::errc::not_connected) {
            return ec;
        }
    }
    const std::string service_string = std::holds_alternative<uint16_t>(service) ? std::to_string(std::get<uint16_t>(service)) : std::get<std::string>(service);
    std::pair<addrinfo*, std::error_condition> addrinfo_result = service_resolver::get_addrinfo(host, service_string, address_family, address_protocol, AI_ADDRCONFIG);
    if (addrinfo_result.first == nullptr) {
        return addrinfo_result.second;
    }
    std::error_condition global_connect_error {};
    for (addrinfo* res_addrinfo = addrinfo_result.first; res_addrinfo != nullptr; res_addrinfo = res_addrinfo->ai_next) {
        auto sock = netlib::socket();
        std::error_condition s_create_error = sock.create(res_addrinfo->ai_family, res_addrinfo->ai_socktype, res_addrinfo->ai_protocol);
        if (s_create_error) {
            freeaddrinfo(addrinfo_result.first);
            return s_create_error; //we already failed creating the socket, oof
        }
        sock.set_nonblocking(true);
        int32_t connect_result = ::connect(sock.get_raw().value(), res_addrinfo->ai_addr, res_addrinfo->ai_addrlen);
        if (connect_result == -1) {
            std::error_condition error_condition = socket_get_last_error();
            if (error_condition != std::errc::operation_in_progress) {
                freeaddrinfo(addrinfo_result.first);
                return error_condition;
            }
        }
        auto connect_error = wait_for_operation(sock.get_raw().value(), OperationClass::write, timeout);
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

std::future<std::error_condition> netlib::client::connect_async(const std::string &host,
                                                                      const std::variant<std::string, uint16_t>& service,
                                                                      netlib::AddressFamily address_family,
                                                                      netlib::AddressProtocol address_protocol,
                                                                      std::chrono::milliseconds timeout) {
    return std::async(std::launch::async, &netlib::client::connect, this,
                      host, service, address_family, address_protocol, timeout);
}
std::future<std::pair<std::size_t, std::error_condition>> netlib::client::send_async(const std::vector<uint8_t> &data, std::optional<std::chrono::milliseconds> timeout) {
    return std::async(std::launch::async, &netlib::client::send, this, data, timeout);
}
std::future<std::pair<std::vector<uint8_t>, std::error_condition>> netlib::client::recv_async(std::size_t byte_count, std::optional<std::chrono::milliseconds> timeout) {
    return std::async(std::launch::async, &netlib::client::recv, this, byte_count, timeout);
}

std::optional<netlib::socket> netlib::client::get_socket() const {
    return _socket;
}

std::error_condition netlib::client::disconnect() {
    if (!_socket.has_value()){
        return std::errc::not_connected;
    }
    _socket->close();
    _socket.reset();

    if (_endpoint_addr) {
      freeaddrinfo(_endpoint_addr);
    }
    return {};
}
bool netlib::client::is_connected() {
    return _socket.has_value();
}

std::pair<std::error_condition, std::chrono::milliseconds> netlib::client::wait_for_operation(socket_t sock, netlib::OperationClass op_class, std::chrono::milliseconds timeout) {
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    timeval tv{.tv_sec = timeout.count() / 1000, .tv_usec=(timeout.count() % 1000) * 1000 };
    fd_set* fdset_ptr_read = ((op_class == OperationClass::read) || (op_class == OperationClass::both)) ? &fdset : nullptr;
    fd_set* fdset_ptr_write = ((op_class == OperationClass::write) || (op_class == OperationClass::both)) ? &fdset : nullptr;
    std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
    int32_t select_res = select(sock + 1, fdset_ptr_read, fdset_ptr_write, nullptr, &tv);
    std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();
    std::chrono::milliseconds ms_taken = std::chrono::duration_cast<std::chrono::milliseconds>((end - start));
    if (select_res == 1)
    {
        int32_t so_error = 0;
        socklen_t len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error == 0) {
            //success
            return {{}, ms_taken};
        }
        return {static_cast<std::errc>(so_error), ms_taken};
    } else if (select_res == 0) {
        //timeout
        return {std::errc::timed_out, ms_taken};
    } else {
        //error
        return {socket_get_last_error(), ms_taken};
    }
}

std::optional<std::string> netlib::client::get_ip_addr() {
  return endpoint_accessor::ip_to_string(_endpoint_addr);
}