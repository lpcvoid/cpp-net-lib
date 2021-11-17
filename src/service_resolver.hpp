#pragma once

#include "Socket.hpp"
#include <netdb.h>
#include <system_error>
#include <utility>

namespace netlib {
  class service_resolver {
  public:
    static std::pair<addrinfo*, std::error_condition> get_addrinfo(const std::string& host,
                                                             const std::string &service,
                                                             AddressFamily address_family,
                                                             AddressProtocol address_protocol);
  };
}

