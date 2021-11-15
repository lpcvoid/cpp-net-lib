//
// Created by lpcvoid on 11/11/2021.
//

#include "Client.hpp"

netlib::client::client() {
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
    std::pair<addrinfo*, std::error_condition> addrinfo_result = get_addrinfo(host, service_string, address_family, address_protocol);
    if (addrinfo_result.first == nullptr) {
        return addrinfo_result.second;
    }
    std::error_condition global_connect_error {};
    for (addrinfo* res_addrinfo = addrinfo_result.first; res_addrinfo != nullptr; res_addrinfo = res_addrinfo->ai_next) {
        auto sock = netlib::Socket();
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
        _socket = sock;
        break;
    }
    freeaddrinfo(addrinfo_result.first);
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

std::optional<netlib::Socket> netlib::client::get_socket() const {
    return _socket;
}

std::error_condition netlib::client::disconnect() {
    if (!_socket.has_value()){
        return std::errc::not_connected;
    }
    _socket->close();
    _socket.reset();
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

std::pair<addrinfo*, std::error_condition> netlib::client::get_addrinfo(const std::string &host, const std::string &service, netlib::AddressFamily address_family, netlib::AddressProtocol address_protocol) {

    addrinfo *result = nullptr;
    addrinfo hints{};
    hints.ai_family = static_cast<int>(address_family);
    hints.ai_socktype = static_cast<int>(address_protocol);
    hints.ai_flags = AI_ADDRCONFIG; //use system config for determining which interfaces to use
    hints.ai_protocol = 0;

    int gai_res = getaddrinfo(host.c_str(), service.c_str(), &hints, &result);
    if (gai_res == 0) {
        return {result, {}};
    }
    //attempt to map these specific errors to posix, if that's impossible we
    //return "best-fit" posix code for indicating what happened
    switch (gai_res) {
        case EAI_AGAIN:{
            return { nullptr, std::errc::resource_unavailable_try_again};
        } break;
        case EAI_FAMILY:{
            return { nullptr, std::errc::address_family_not_supported};
        } break;
        case EAI_ADDRFAMILY:
        case EAI_NODATA:
        case EAI_NONAME:
        case EAI_FAIL: {
            return { nullptr, std::errc::network_unreachable};
        } break;
        case EAI_MEMORY: {
            return { nullptr, std::errc::not_enough_memory};
        } break;
        case EAI_SYSTEM: {
            return { nullptr, socket_get_last_error()};
        } break;
        case EAI_SOCKTYPE:
        case EAI_SERVICE: //this is perhaps debatable?
        case EAI_BADFLAGS: {
            return { nullptr, std::errc::invalid_argument};
        } break;
        default: {
            // we missuse the "not_supported" posix error type to indicate
            // that we don't recognize the error type
            return { nullptr, std::errc::not_supported};
        }

    }
}
