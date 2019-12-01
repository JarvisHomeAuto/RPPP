cmake_minimum_required(VERSION 3.2)
project(all_tests)
add_subdirectory(lib/googletest)

aux_source_directory(src TEST_FILES)
add_executable(all_tests
    ${TEST_FILES}
)

target_include_directories(all_tests
    PRIVATE lib/googletest/googletest/include
    ../${PROJECT_INCLUDE_DIR}
)

target_compile_options(all_tests
    PUBLIC -Wall -g -O0 -std=c++17
)

target_link_libraries(all_tests
    gtest
)