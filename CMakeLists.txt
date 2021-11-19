cmake_minimum_required(VERSION 3.20)
project(netlib)
set(CMAKE_CXX_STANDARD 20)

set(NETLIB_SRC src/socket.hpp src/error.hpp src/client.cpp src/client.hpp
        src/server.cpp src/server.hpp src/service_resolver.cpp src/service_resolver.hpp
        src/endpoint_accessor.hpp src/thread_pool.cpp src/thread_pool.hpp)

# doctest
include_directories(3rdparty)
add_subdirectory(tests)
add_subdirectory(examples)

add_library(netcpp STATIC ${NETLIB_SRC})
