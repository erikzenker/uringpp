cmake_minimum_required(VERSION 3.5)
project(cp)

# dependencies
if(NOT TARGET uringpp::uringpp)
    find_package(uringpp CONFIG REQUIRED)
endif()

# target defintion
add_executable(ncp main.cpp)

target_link_libraries(ncp
        PRIVATE
        uringpp::uringpp)