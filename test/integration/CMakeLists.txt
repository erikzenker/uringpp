cmake_minimum_required(VERSION 3.5)
project(uringppIntegrationTests)

# dependencies
enable_testing()
if(NOT TARGET uringpp::uringpp)
    find_package(uringpp CONFIG REQUIRED)
endif()

# target defintion
add_executable(uringppIntegrationTests 
        main.cpp 
        basic_setup.cpp
)

target_link_libraries(uringppIntegrationTests
        PRIVATE
          uringpp::uringpp
          gtest_main
)

add_test(NAME uringpp.IntegrationTests
        COMMAND uringppIntegrationTests
)
