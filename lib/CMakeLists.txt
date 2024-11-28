
add_library(liblegday SHARED liblegday.cpp)
target_include_directories(liblegday PUBLIC ${YOUR_DIRECTORY})

set_target_properties(liblegday PROPERTIES PUBLIC_HEADER "legday.h")
INSTALL(TARGETS liblegday
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include
)

# Add a command-line utility
add_executable(legday main.cpp)
target_link_libraries(legday PUBLIC liblegday)
set_target_properties(legday PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


# Add testing
find_package(GTest REQUIRED)
add_executable(legday_test test_legday.cpp)
target_link_libraries(legday_test PUBLIC liblegday GTest::GTest GTest::Main)
add_test(legday_test legday_test)
