cmake_minimum_required(VERSION 3.10)
project(legday)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Add subdirectories for library and utility
add_subdirectory(lib)
add_subdirectory(utility)

# Add testing
enable_testing()
add_subdirectory(test)
