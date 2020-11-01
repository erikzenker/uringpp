cmake_minimum_required(VERSION 3.5)
project(tcp_echo_poll)

# dependencies
if(NOT TARGET uringpp::uringpp)
    find_package(uringpp CONFIG REQUIRED)
endif()

# target defintion
add_executable(tcp_echo_poll main.cpp)

target_link_libraries(tcp_echo_poll
        PRIVATE
        uringpp::uringpp)