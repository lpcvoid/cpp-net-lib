#pragma once

#include "socket.hpp"
#include <netdb.h>
#include <system_error>
#include <utility>

namespace netlib {
  class service_resolver {
  public:
    static std::pair<addrinfo*, std::error_condition> get_addrinfo( std::optional<std::string> host,
                                                                    std::optional<std::string> service,
                                                                    AddressFamily address_family,
                                                                    AddressProtocol address_protocol,
                                                                    uint32_t flags = 0);
  };
}

