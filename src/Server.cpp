//
// Created by lpcvoid on 14/11/2021.
//

#include "Server.hpp"
#include "service_resolver.hpp"
#include <cassert>
#include <csignal>

netlib::server::server() {}

std::error_condition netlib::server::create(const std::string &bind_host,
                       const std::variant<std::string, uint16_t> &service,
                                            AddressFamily address_family,
                                            AddressProtocol address_protocol) {

  if (_listener_sock.has_value()) {
    this->close();
  }

  const std::string service_string = std::holds_alternative<uint16_t>(service) ? std::to_string(std::get<uint16_t>(service)) : std::get<std::string>(service);
  std::pair<addrinfo*, std::error_condition> addrinfo_result = service_resolver::get_addrinfo(std::nullopt, service_string, address_family, address_protocol, AI_PASSIVE);

  if (addrinfo_result.first == nullptr) {
    return addrinfo_result.second;
  }

  auto close_and_free = [&](){
    this->close();
    freeaddrinfo(addrinfo_result.first);
  };

  for (addrinfo* res_addrinfo = addrinfo_result.first; res_addrinfo != nullptr; res_addrinfo = res_addrinfo->ai_next) {
    _listener_sock = netlib::Socket();
    std::error_condition s_create_error = _listener_sock->create(res_addrinfo->ai_family, res_addrinfo->ai_socktype, res_addrinfo->ai_protocol);
    if (s_create_error) {
      close_and_free();
      continue;
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
    }
    //all went well
    break;
  }
  if (_listener_sock) {
    _server_active = true;
    _accept_thread = std::thread(&server::accept_func, this);
    _processor_thread = std::thread(&server::processing_func, this);
    return {};
  }
  return socket_get_last_error();
}

std::size_t netlib::server::get_client_count() {
  std::lock_guard<std::mutex> lock(_mutex);
  return _clients.size();
}

void netlib::server::processing_func() {
  while (_server_active) {
    fd_set fdset;
    socket_t highest_fd = 0;
    FD_ZERO(&fdset);
    {
      std::lock_guard<std::mutex> lock(_mutex);
      for (auto& client : _clients) {
        socket_t fd = client.socket.get_raw().value();
        if (highest_fd < fd){
          highest_fd = fd;
        }
        FD_SET(fd, &fdset);
      }
    }
    //we want the timeout to be fairly low, so that we avoid situations where
    //we have a new client in _clients, but are not monitoring it yet - that
    //would mean we have a "hardcoded" delay in servicing a new clients packets
    timeval tv{.tv_sec = 0, .tv_usec= 50 * 1000}; //50ms
    int32_t select_res = ::select(highest_fd + 1, &fdset, nullptr, nullptr, &tv);
    if (select_res > 0) {
      std::vector<client_endpoint> client_refs(select_res);
      uint32_t index = 0;
      {
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto& client : _clients) {
          socket_t fd = client.socket.get_raw().value();
          if (FD_ISSET(fd, &fdset)){
            client_refs[index++] = client;
          }
        }
      }
      //TODO: remove this later on
      assert(index == select_res);
      //TODO: threadpool, don't spawn a thread each time
      for (auto& client_to_recv : client_refs) {
          std::thread(&server::handle_client, this, client_to_recv).detach();
      }

    }
  }
}

void netlib::server::accept_func() {
  client_endpoint new_endpoint;
  while (_server_active) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    new_endpoint.addr_len = sizeof(addrinfo);
    int32_t status = ::accept(_listener_sock->get_raw().value(), new_endpoint.addr, &new_endpoint.addr_len);
    if (status > 0) {
      new_endpoint.socket.set_raw(status);
      {
        std::lock_guard<std::mutex> lock(_mutex);
        _clients.push_back(new_endpoint);
      }
      if (_cb_onconnect) {
        std::vector<uint8_t> greeting = _cb_onconnect(new_endpoint);
        if (!greeting.empty()) {
          int32_t send_result = ::send(new_endpoint.socket.get_raw().value(), greeting.data(), greeting.size(), 0);
          if ((send_result != greeting.size()) && (_cb_on_error)) {
            _cb_on_error(new_endpoint, socket_get_last_error());
          }
        }
      }
    }
  }
}
void netlib::server::close() {
  _server_active = false;
  if (_accept_thread.joinable()) {
    _accept_thread.join();
  }
  if (_processor_thread.joinable()){
    _processor_thread.join();
  }
  if (_listener_sock.has_value()) {
    _listener_sock->close();
    _listener_sock.reset();
  }
}
std::error_condition
netlib::server::handle_client(netlib::client_endpoint endpoint) {
  std::vector<uint8_t> total_buffer;
  std::array<uint8_t, 2048> buffer{};
  while (int32_t recv_res = ::recv(endpoint.socket.get_raw().value(), buffer.data(), buffer.size(), MSG_WAITALL) > 0) {
    total_buffer.insert(total_buffer.end(), buffer.begin(), buffer.begin() + recv_res);
  }
  if (_cb_on_recv) {
    std::vector<uint8_t> response = _cb_on_recv(endpoint, total_buffer);
    if (!response.empty()) {
      int32_t send_result = ::send(endpoint.socket.get_raw().value(), response.data(), response.size(), 0);
      if ((send_result != response.size()) && (_cb_on_error)) {
        _cb_on_error(endpoint, socket_get_last_error());
      }
    }
  }

}
netlib::server::~server() {
  close();
}
