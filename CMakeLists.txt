cmake_minimum_required(VERSION 3.10)
project(legday)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")


# Add subdirectories for library and utility
add_subdirectory(lib)
add_subdirectory(utility)

# Add testing
enable_testing()
add_subdirectory(test)
