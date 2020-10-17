cmake_minimum_required(VERSION 3.5)
project(uringpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -Werror -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Werror -Wall -Wextra -Wpedantic -g3 -O0")

add_subdirectory(integration)