//
// Created by lpcvoid on 14/11/2021.
//

#include "Server.hpp"

netlib::server::server() {}
std::error_condition
netlib::server::create(const std::string &bind_host,
                       const std::variant<std::string, uint16_t> &service) {
  return std::error_condition();
}
std::size_t netlib::server::get_client_count() {
  std::lock_guard<std::mutex> lock(_mutex);
  return _clients.size();
}
