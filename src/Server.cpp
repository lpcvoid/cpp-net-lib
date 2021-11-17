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

  const std::string service_string = std::holds_alternative<uint16_t>(service) ? std::to_string(std::get<uint16_t>(service)) : std::get<std::string>(service);
  std::pair<addrinfo*, std::error_condition> addrinfo_result = service_resolver::get_addrinfo(bind_host, service_string, address_family, address_protocol, AI_PASSIVE);

}

std::size_t netlib::server::get_client_count() {
  std::lock_guard<std::mutex> lock(_mutex);
  return _clients.size();
}

void netlib::server::processing_func() {

}

void netlib::server::accept_func() {

}
