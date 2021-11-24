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

    static std::error_condition socket_get_last_error(){
#ifdef _WIN32
      return WASGetLastError();
#else
      return std::make_error_condition(static_cast<std::errc>(errno));
#endif
}

    enum class AddressFamily {IPv4 = AF_INET, IPv6 = AF_INET6, unspecified = AF_UNSPEC};
    enum class AddressProtocol {TCP = SOCK_STREAM, UDP = SOCK_DGRAM};
    enum class OperationClass {read = 1, write = 2, both = 3};

    class socket {
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
      socket() = default;

        bool is_valid() {
            return _socket.has_value() && _socket.value() != INVALID_SOCKET;
        }

        [[nodiscard]] std::optional<socket_t> get_raw() const {
            return _socket.value();
        }

        bool set_nonblocking(bool nonblocking = true) {
#ifdef _WIN32
            u_long mode = static_cast<u_long>(nonblocking);
            return ioctlsocket(_socket.value(), FIONBIO, &mode) == 0;
#else
            return fcntl(_socket.value(), F_SETFL, fcntl(_socket.value(), F_GETFL, 0) | (nonblocking ? O_NONBLOCK : 0)) == 0;
#endif
        }

        bool set_reuseaddr(bool reuseaddr = true){
#ifdef _WIN32
            int32_t val = static_cast<int32_t>(reuseaddr);
            return setsockopt(_socket.value(), SOL_SOCKET, SO_REUSEADDR,
                              reinterpret_cast<char*>(&val), sizeof(val)) == 0;
#else
            auto mode = static_cast<int32_t>(reuseaddr);
            return setsockopt(_socket.value(), SOL_SOCKET, SO_REUSEADDR, &mode, sizeof(int32_t)) == 0;
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

        void set_raw(socket_t sock) {
          _socket = sock;
        }

        void close() {
            if (_socket) {
#ifdef _WIN32
                closesocket(_socket.value());
#else
                ::close(_socket.value());
#endif
                _socket.reset();
            }
        }
    };
}