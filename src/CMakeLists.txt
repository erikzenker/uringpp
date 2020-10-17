cmake_minimum_required(VERSION 3.5)
project(uringpp VERSION 0.0.1)

# library definition
add_library(uringpp INTERFACE)

install(TARGETS uringpp EXPORT uringppTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include)
install(EXPORT uringppTargets
        DESTINATION lib/cmake/uringpp
        FILE uringppTargets.cmake
        NAMESPACE uringpp::)
install(DIRECTORY include/uringpp
        DESTINATION include)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(uringppConfigVersion.cmake
        COMPATIBILITY SameMajorVersion)
install(FILES uringppConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/uringppConfigVersion.cmake
        DESTINATION lib/cmake/uringpp)

target_compile_features(uringpp INTERFACE cxx_std_17)
target_include_directories(uringpp INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

# TODO: Finding the uring library should be part of a cmake find module    
add_library(Uring::Uring INTERFACE IMPORTED)
set_target_properties(Uring::Uring PROPERTIES INTERFACE_LINK_LIBRARIES "/usr/lib/liburing.so")
set_target_properties(Uring::Uring PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "/usr/include/")
        
target_link_libraries(uringpp INTERFACE
        Uring::Uring)
add_library(uringpp::uringpp ALIAS uringpp)
