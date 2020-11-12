cmake_minimum_required(VERSION 3.5)
project(ranges_cat)

# dependencies
if(NOT TARGET uringpp::uringpp)
    find_package(uringpp CONFIG REQUIRED)
endif()

find_package (Threads)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")

# target defintion
add_executable(ranges_cat main.cpp)

target_link_libraries(ranges_cat
        PRIVATE
        uringpp::uringpp
        Threads::Threads
        )