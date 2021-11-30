#pragma once

#include <optional>
#include <system_error>

#ifdef _WIN32
// libs
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "AdvApi32.lib")

// headers
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>
#include <iphlpapi.h>
#include <stdint.h>
#include <ws2tcpip.h>

// defines
using socket_t = SOCKET;
using socklen_t = int32_t;
using ssize_t = signed long long int;
//we don't have signals on windows
#define MSG_NOSIGNAL 0
//poll implementation
#define poll_syscall ::WSAPoll
#else
// headers
#include <arpa/inet.h>
#include <chrono>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
// defines
using socket_t = int32_t;
#define poll_syscall ::poll
// this is actually a nice one to have
#define INVALID_SOCKET (-1)
#ifdef __APPLE__
#define MSG_NOSIGNAL SO_NOSIGPIPE
#endif
#endif

namespace netlib {

static std::error_condition socket_get_last_error()
{
    std::error_code ec;
#ifdef _WIN32
    ec = std::error_code(::GetLastError(), std::system_category());
#else
    ec = std::error_code(errno, std::system_category());
#endif
    return ec.default_error_condition();
}

enum class AddressFamily { IPv4 = AF_INET, IPv6 = AF_INET6, unspecified = AF_UNSPEC };
enum class AddressProtocol { TCP = SOCK_STREAM, UDP = SOCK_DGRAM };
enum class OperationClass { read = 1, write = 2, both = 3 };

class socket {
private:
    std::optional<socket_t> _socket = std::nullopt;

public:
    socket() = default;

    static bool initialize_system()
    {
#ifdef _WIN32
        static bool initialized = false;
        if (!initialized) {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
            initialized = true;
        }
#endif
        return true;
    }

    [[nodiscard]] bool is_valid() const
    {
        return _socket.has_value() && _socket.value() != INVALID_SOCKET;
    }

    [[nodiscard]] std::optional<socket_t> get_raw() const
    {
        return _socket.value();
    }

    bool set_nonblocking(bool nonblocking = true)
    {
#ifdef _WIN32
        u_long mode = static_cast<u_long>(nonblocking);
        return ioctlsocket(_socket.value(), FIONBIO, &mode) == 0;
#else
        return fcntl(_socket.value(), F_SETFL, fcntl(_socket.value(), F_GETFL, 0) | (nonblocking ? O_NONBLOCK : 0)) == 0;
#endif
    }

    std::optional<std::size_t> get_recv_buffer_size() {
#ifdef _WIN32
#else
        int32_t buffer_size = 0;
        socklen_t response_size = sizeof(buffer_size);
        if (getsockopt(_socket.value(), SOL_SOCKET, SO_RCVBUF, &buffer_size, &response_size) == 0) {
            return buffer_size;
        }
        return std::nullopt;
#endif
    }

    std::optional<std::size_t> get_send_buffer_size() {
#ifdef _WIN32
#else
        int32_t buffer_size = 0;
        socklen_t response_size = sizeof(buffer_size);
        if (getsockopt(_socket.value(), SOL_SOCKET, SO_SNDBUF, &buffer_size, &response_size) == 0) {
            return buffer_size;
        }
        return std::nullopt;
#endif
    }

    bool set_recv_buffer_size(std::size_t buf_size) {
#ifdef _WIN32
#else
        auto buffer_size = static_cast<int32_t>(buf_size);
        return (setsockopt(_socket.value(), SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size)) == 0);
#endif
    }

    bool set_send_buffer_size(std::size_t buf_size) {
#ifdef _WIN32
#else
        auto buffer_size = static_cast<int32_t>(buf_size);
        return (setsockopt(_socket.value(), SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size)) == 0);
#endif
    }

    bool set_nagle(bool enable) {
#ifdef _WIN32
        int32_t val = static_cast<int32_t>(enable);
        return setsockopt(_socket.value(), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&val), sizeof(val)) == 0;
#else
        auto mode = static_cast<int32_t>(enable);
        return setsockopt(_socket.value(), IPPROTO_TCP, TCP_NODELAY, &mode, sizeof(int32_t)) == 0;
#endif
    }

    bool set_reuseaddr(bool reuseaddr = true)
    {
#ifdef _WIN32
        int32_t val = static_cast<int32_t>(reuseaddr);
        return setsockopt(_socket.value(), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&val), sizeof(val)) == 0;
#else
        auto mode = static_cast<int32_t>(reuseaddr);
        return setsockopt(_socket.value(), SOL_SOCKET, SO_REUSEADDR, &mode, sizeof(int32_t)) == 0;
#endif
    }

    std::error_condition create(int32_t domain, int32_t stype, int32_t protocol)
    {
        initialize_system();
        socket_t new_socket = ::socket(domain, stype, protocol);
        if (new_socket == INVALID_SOCKET) {
            return socket_get_last_error();
        }
        _socket = new_socket;
        if (!set_nagle(false)) {
            return socket_get_last_error();
        }
        return {};
    }

    void set_raw(socket_t sock)
    {
        _socket = sock;
    }

    void close()
    {
        if (_socket) {
#ifdef _WIN32
            closesocket(_socket.value());
#else
            ::close(_socket.value());
#endif
            _socket.reset();
        }
    }

    bool operator==(const socket &rhs) const
    {
        if (!_socket.has_value() && !rhs.get_raw().has_value()) {
            return true; // empty sockets are equal
        }

        if (_socket.has_value() ^ rhs.get_raw().has_value()) {
            return false; // one is empty, other is not, thus not equal
        }

        return _socket.value() == rhs._socket.value();
    }
    bool operator!=(const socket &rhs) const
    {
        return !(rhs == *this);
    }
};
} // namespace netlib