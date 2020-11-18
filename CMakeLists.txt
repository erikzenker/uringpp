cmake_minimum_required(VERSION 3.5)
project(uringpp)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")

add_subdirectory(vendor/googletest)

# add project code
add_subdirectory(src)

# add unit tests
enable_testing()
add_subdirectory(test)

add_subdirectory(example)