cmake_minimum_required(VERSION 3.5)
project(cat)

# dependencies
if(NOT TARGET uringpp::uringpp)
    find_package(uringpp CONFIG REQUIRED)
endif()

find_package (Threads)
find_package(Boost REQUIRED COMPONENTS thread)
find_package(cppcoro REQUIRED)
find_package(asyncly REQUIRED)

# target defintion
add_executable(cat main.cpp)

target_link_libraries(cat
        PRIVATE
        uringpp::uringpp
)