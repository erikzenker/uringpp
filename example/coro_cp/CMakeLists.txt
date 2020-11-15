cmake_minimum_required(VERSION 3.5)
project(coro_cp)

# dependencies
if(NOT TARGET uringpp::uringpp)
    find_package(uringpp CONFIG REQUIRED)
endif()

# target defintion
add_executable(coro_cp main.cpp)

target_link_libraries(coro_cp
        PRIVATE
        uringpp::uringpp
        )