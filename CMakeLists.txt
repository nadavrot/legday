cmake_minimum_required(VERSION 3.10)
project(legday)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Os")

enable_testing()

# Add subdirectories for library and utility
add_subdirectory(lib)

