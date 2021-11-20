cmake_minimum_required(VERSION 3.20)
project(netlib)
set(CMAKE_CXX_STANDARD 20)

set(NETLIB_SRC src/netlib.hpp src/socket.hpp src/error.hpp src/client.hpp
        src/server.hpp src/service_resolver.hpp
        src/endpoint_accessor.hpp src/thread_pool.hpp)

# doctest
include_directories(3rdparty)
add_subdirectory(tests)
add_subdirectory(examples)
