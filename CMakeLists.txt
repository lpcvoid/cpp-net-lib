cmake_minimum_required(VERSION 3.15)
project(netlib)
set(CMAKE_CXX_STANDARD 20)

# we want the maximum amount of compiler bickering
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

find_package(Threads)

set(NETLIB_SRC
        src/netlib.hpp
        src/socket.hpp
        src/client.hpp
        src/server.hpp
        src/service_resolver.hpp
        src/endpoint_accessor.hpp
        src/thread_pool.hpp
        src/socket_operations.hpp)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build example programs" ON)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/doctest/CMakeLists.txt")
    set(BUILD_TESTS OFF)
endif()

if(BUILD_TESTS)
    # doctest
    include_directories(doctest)
    add_subdirectory(tests)
endif()

if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()