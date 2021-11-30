#pragma once

#include "socket.hpp"
#include <system_error>
#include <utility>

namespace netlib {
class service_resolver {
public:
    static std::pair<addrinfo *, std::error_condition> get_addrinfo(std::optional<std::string> host, std::optional<std::string> service,
                                                                    AddressFamily address_family, AddressProtocol address_protocol,
                                                                    uint32_t flags = 0)
    {
        addrinfo *result = nullptr;
        addrinfo hints{};
        hints.ai_family = static_cast<int32_t>(address_family);
        hints.ai_socktype = static_cast<int32_t>(address_protocol);
        hints.ai_flags = static_cast<int32_t>(flags); // use system config for determining which interfaces to use
        hints.ai_protocol = 0;

        const char *host_str = (host.has_value()) ? host.value().c_str() : nullptr;
        const char *service_str = (service.has_value()) ? service.value().c_str() : nullptr;
        int gai_res = getaddrinfo(host_str, service_str, &hints, &result);
        if (gai_res == 0) {
            return {result, {}};
        }
        // attempt to map these specific errors to posix, if that's impossible we
        // return "best-fit" posix code for indicating what happened
        switch (gai_res) {
        case EAI_AGAIN: {
            return {nullptr, std::errc::resource_unavailable_try_again};
        }
        case EAI_FAMILY: {
            return {nullptr, std::errc::address_family_not_supported};
        }
#ifndef _WIN32
        case EAI_ADDRFAMILY:
#endif
        case EAI_NODATA:
        case EAI_FAIL: {
            return {nullptr, std::errc::network_unreachable};
        }
        case EAI_MEMORY: {
            return {nullptr, std::errc::not_enough_memory};
        }
#ifndef _WIN32
        case EAI_SYSTEM: {
            return {nullptr, socket_get_last_error()};
        }
#endif
        case EAI_SOCKTYPE:
        case EAI_SERVICE: // this is perhaps debatable?
        case EAI_BADFLAGS: {
            return {nullptr, std::errc::invalid_argument};
        }
        default: {
            // we missuse the "not_supported" posix error type to indicate
            // that we don't recognize the error type
            return {nullptr, std::errc::not_supported};
        }
        }
    }
};
} // namespace netlib
