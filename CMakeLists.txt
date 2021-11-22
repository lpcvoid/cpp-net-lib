cmake_minimum_required(VERSION 3.15)
project(netlib)
set(CMAKE_CXX_STANDARD 20)

find_package(Threads)

set(NETLIB_SRC
        src/netlib.hpp
        src/socket.hpp
        src/error.hpp
        src/client.hpp
        src/server.hpp
        src/service_resolver.hpp
        src/endpoint_accessor.hpp
        src/thread_pool.hpp)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_TESTS)
    # doctest
    include_directories(3rdparty)
    add_subdirectory(tests)
endif()

if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()