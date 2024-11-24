find_package(GTest REQUIRED)

add_executable(legday_test test_legday.cpp)
target_link_libraries(legday_test PUBLIC legday_lib GTest::GTest GTest::Main)
add_test(legday_test legday_test)
