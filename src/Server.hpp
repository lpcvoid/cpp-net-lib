#pragma once

#include "Socket.hpp"
#include <optional>
namespace netlib {
    class Server {
    private:
        std::optional<netlib::Socket> _listener_sock;

    };
}

