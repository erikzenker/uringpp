cmake_minimum_required(VERSION 3.5)
project(tcp_echo)

# dependencies
if(NOT TARGET uringpp::uringpp)
    find_package(uringpp CONFIG REQUIRED)
endif()

# target defintion
add_executable(tcp_echo main.cpp)

target_link_libraries(tcp_echo
        PRIVATE
        uringpp::uringpp)