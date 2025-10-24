file(GLOB TEST_SOURCES *.cpp)
add_executable(tests ${TEST_SOURCES})
target_include_directories(tests PRIVATE ../include)
add_test(NAME all_tests COMMAND tests)
