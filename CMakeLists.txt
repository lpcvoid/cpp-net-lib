cmake_minimum_required(VERSION 3.20)
project(getcat)
set(CMAKE_CXX_STANDARD 20)

set(NETLIB_SRC src/Socket.hpp src/Error.hpp src/Client.cpp src/Client.hpp
                  src/Server.cpp src/Server.hpp)

# doctest
include_directories(3rdparty)
add_subdirectory(tests)

add_library(netcpp STATIC ${NETLIB_SRC})
