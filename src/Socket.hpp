#pragma once

#include <system_error>
#include <optional>

#ifdef _WIN32
//libs
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "AdvApi32.lib")

    //headers
    #include <winsock2.h>
    #include <iphlpapi.h>
    #include <windows.h>
    #include <ws2tcpip.h>
  #include <stdint.h>

//defines
using socket_t = SOCKET;
using socklen_t = int32_t;
using ssize_t = signed long long int;
#else
//headers
#include "Error.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
//defines
using socket_t = int32_t;
//this is actually a nice one to have
#define INVALID_SOCKET (-1)
#endif

namespace netlib {

    enum class AddressFamily {IPv4 = AF_INET, IPv6 = AF_INET6, unspecified = AF_UNSPEC};
    enum class AddressProtocol {TCP = SOCK_STREAM, UDP = SOCK_DGRAM};
    enum class OperationClass {read = 1, write = 2, both = 3};

    class Socket {
    private:
        std::optional<socket_t> _socket = std::nullopt;

        static bool initialize_system() {
#ifdef _WIN32
            static bool initialized = false;
            if (!initialized) {
                WSADATA wsaData;
                WSAStartup(MAKEWORD(2,2), &wsaData);
                initialized = true;
            }
#else
            return true;
#endif
        }
    public:
        Socket() = default;

        bool is_valid() {
            return _socket.has_value();
        }

        [[nodiscard]] std::optional<socket_t> get_raw() const {
            return _socket.value();
        }

        bool set_nonblocking(bool nonblocking = true) {
#ifdef _WIN32
            uint32_t mode = static_cast<uint32_t>(nonblocking);
            return ioctlsocket(sockfd, FIONBIO, &mode) == 0;
#else
            return fcntl(_socket.value(), F_SETFL, fcntl(_socket.value(), F_GETFL, 0) | (nonblocking ? O_NONBLOCK : 0)) == 0;
#endif
        }

        bool set_reuseaddr(bool reuseaddr = true){
            auto mode = static_cast<int32_t>(reuseaddr);
            return setsockopt(_socket.value(), SOL_SOCKET, SO_REUSEADDR, &mode, sizeof(int32_t)) == 0;
        }

        bool set_recv_timeout(std::chrono::milliseconds ms){
#ifdef _WIN32
            int32_t val = ms.count();
            return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                              reinterpret_cast<char*>(&val), sizeof(val));
#else
            timeval tv{.tv_sec = ms.count() / 1000, .tv_usec=(ms.count() % 1000) * 1000 };
            return setsockopt(_socket.value(), SOL_SOCKET, SO_RCVTIMEO,
                              reinterpret_cast<void*>(&tv), sizeof(timeval)) == 0;
#endif
        }

        bool set_send_timeout(std::chrono::milliseconds ms){
#ifdef _WIN32
            int32_t val = ms.count();
            return setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
                              reinterpret_cast<char*>(&val), sizeof(val));
#else
            timeval tv{.tv_sec = ms.count() / 1000, .tv_usec=(ms.count() % 1000) * 1000 };
            return setsockopt(_socket.value(), SOL_SOCKET, SO_SNDTIMEO,
                              reinterpret_cast<void*>(&tv), sizeof(timeval)) == 0;
#endif
        }


        std::error_condition create(int32_t domain, int32_t stype, int32_t protocol) {
            initialize_system();
            socket_t new_socket = ::socket(domain, stype, protocol);
            if (new_socket == INVALID_SOCKET) {
                return socket_get_last_error();
            }
            _socket = new_socket;
            return {};
        }

        void close() {
            if (_socket) {
#ifdef _WIN32
                closesocket(sockfd);
#else
                ::close(_socket.value());
#endif
            }
        }
    };
}