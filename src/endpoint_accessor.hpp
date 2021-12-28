#pragma once
#include "socket.hpp"
#include <array>
#include <cstring>
#include <optional>
#include <string>

namespace netlib {
class endpoint_accessor {
public:
    static std::optional<std::string> ip_to_string(addrinfo *ai)
    {
        if (!ai) {
            return std::nullopt;
        }
        return ip_to_string(ai->ai_addr, ai->ai_addrlen);
    }
    static std::optional<std::string> ip_to_string(sockaddr sa, socklen_t socklen)
    {
        return ip_to_string(&sa, socklen);
    }
    static std::optional<std::string> ip_to_string(sockaddr *sa, socklen_t socklen)
    {
        if (!sa || !socklen) {
            return std::nullopt;
        }
        std::string ip;
        if (socklen == sizeof(sockaddr_in)) {
            std::array<char, INET_ADDRSTRLEN> ip_addr{};
            inet_ntop(AF_INET, &(((sockaddr_in *)sa)->sin_addr), ip_addr.data(), INET_ADDRSTRLEN);
            ip.resize(std::strlen(ip_addr.data()));
            std::strcpy(ip.data(), ip_addr.data());
            return ip;
        } else if (socklen == sizeof(sockaddr_in6)) {
            std::array<char, INET6_ADDRSTRLEN> ip_addr{};
            inet_ntop(AF_INET6, &(((sockaddr_in6 *)sa)->sin6_addr), ip_addr.data(), INET6_ADDRSTRLEN);
            ip.resize(std::strlen(ip_addr.data()));
            std::strcpy(ip.data(), ip_addr.data());
            return ip;
        }
        return std::nullopt;
    }
    static std::optional<uint16_t> get_port(addrinfo *ai)
    {
        if (ai->ai_family == AF_INET) {
            return ntohs(((sockaddr_in *)ai->ai_addr)->sin_port);
        } else if (ai->ai_family == AF_INET6) {
            return ntohs(((sockaddr_in6 *)ai->ai_addr)->sin6_port);
        }
        return std::nullopt;
    }
};
} // namespace netlib