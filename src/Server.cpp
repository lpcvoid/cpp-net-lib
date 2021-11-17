//
// Created by lpcvoid on 14/11/2021.
//

#include "Server.hpp"
#include "service_resolver.hpp"

netlib::server::server() {}

std::error_condition netlib::server::create(const std::string &bind_host,
                       const std::variant<std::string, uint16_t> &service,
                                            AddressFamily address_family,
                                            AddressProtocol address_protocol) {

  if (_listener_sock.has_value()) {
    this->close();
  }

  const std::string service_string = std::holds_alternative<uint16_t>(service) ? std::to_string(std::get<uint16_t>(service)) : std::get<std::string>(service);
  std::pair<addrinfo*, std::error_condition> addrinfo_result = service_resolver::get_addrinfo(bind_host, service_string, address_family, address_protocol, AI_PASSIVE);

  if (addrinfo_result.first == nullptr) {
    return addrinfo_result.second;
  }

  auto close_and_free = [&](){
    if (_listener_sock.has_value()) {
      _listener_sock->close();
      _listener_sock.reset();
    }
    freeaddrinfo(addrinfo_result.first);
  };

  for (addrinfo* res_addrinfo = addrinfo_result.first; res_addrinfo != nullptr; res_addrinfo = res_addrinfo->ai_next) {
    _listener_sock = netlib::Socket();
    std::error_condition s_create_error = _listener_sock->create(res_addrinfo->ai_family, res_addrinfo->ai_socktype, res_addrinfo->ai_protocol);
    if (s_create_error) {
      close_and_free();
      return s_create_error;
    }
    _listener_sock->set_nonblocking(true);
    if (address_protocol == AddressProtocol::TCP) {
      int32_t res = ::bind(_listener_sock->get_raw().value(), res_addrinfo->ai_addr, res_addrinfo->ai_addrlen);
      if (res < 0) {
        close_and_free();
        continue;
      }
      res = ::listen(_listener_sock->get_raw().value(), _accept_queue_size);
      if (res < 0) {
        close_and_free();
        continue;
      }

      _accept_thread = std::thread(([this](){
        client_endpoint new_endpoint;
        while (_server_active) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          int32_t status = ::accept(_listener_sock->get_raw().value(), new_endpoint.addr, &new_endpoint.addr_len);
          if (status > 0) {
            new_endpoint.socket.set_raw(status);
            _clients.push_back(new_endpoint);
            if (_cb_onconnect) {
              _cb_onconnect(new_endpoint);
            }
            new_endpoint.addr_len = sizeof(addrinfo);
          }
        }
      }));
    }

    //all went well
    break;
  }

  return {};
}

std::size_t netlib::server::get_client_count() {
  std::lock_guard<std::mutex> lock(_mutex);
  return _clients.size();
}

void netlib::server::processing_func() {

}

void netlib::server::accept_func() {

}
void netlib::server::close() {
  _server_active = false;
  if (_accept_thread.joinable()) {
    _accept_thread.join();
  }
  if (_listener_sock.has_value()) {
    _listener_sock->close();
    _listener_sock.reset();
  }
}
