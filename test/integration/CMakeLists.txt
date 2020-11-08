cmake_minimum_required(VERSION 3.5)
project(uringppIntegrationTests)

# dependencies
enable_testing()
if(NOT TARGET uringpp::uringpp)
    find_package(uringpp CONFIG REQUIRED)
endif()

# Added because asio fails to compile with concepts
# See https://github.com/boostorg/asio/issues/312
add_definitions(-DBOOST_ASIO_DISABLE_CONCEPTS)

# target defintion
add_executable(uringppIntegrationTests 
        main.cpp 
        queue_tests.cpp
        readv_tests.cpp
        writev_tests.cpp
        buffer_tests.cpp
        RingServiceTests.cpp
)

target_link_libraries(uringppIntegrationTests
        PRIVATE
          uringpp::uringpp
          gtest_main
)

add_test(NAME uringpp.IntegrationTests
        COMMAND uringppIntegrationTests
)
