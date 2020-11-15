cmake_minimum_required(VERSION 3.5)
project(coro_cat)

# dependencies
if(NOT TARGET uringpp::uringpp)
    find_package(uringpp CONFIG REQUIRED)
endif()

# target defintion
add_executable(coro_cat main.cpp)

target_link_libraries(coro_cat
        PRIVATE
        uringpp::uringpp
        )