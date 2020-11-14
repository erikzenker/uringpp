cmake_minimum_required(VERSION 3.5)
project(ranges_cat)

# dependencies
if(NOT TARGET uringpp::uringpp)
    find_package(uringpp CONFIG REQUIRED)
endif()

find_package (Threads)
find_package(Boost REQUIRED COMPONENTS thread)
find_package(cppcoro REQUIRED)
find_package(asyncly REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines -fsanitize=address")

# target defintion
add_executable(ranges_cat main.cpp)

target_link_libraries(ranges_cat
        PRIVATE
        uringpp::uringpp
        Threads::Threads
        cppcoro::cppcoro
        asyncly
        asyncly_test_utils
        )