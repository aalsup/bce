cmake_minimum_required(VERSION 3.21)
# Fix behavior of CMAKE_CXX_STANDARD when targeting macOS.
if (POLICY CMP0025)
    cmake_policy(SET CMP0025 NEW)
endif ()
set (CMAKE_CXX_STANDARD 11)

project(tests)

add_executable(
    tests
    linked_list_tests.cpp
    ../linked_list.c
)

set_target_properties(tests PROPERTIES LINKER_LANGUAGE CXX)

#find_package(Catch2 2 REQUIRED)

include_directories(/usr/local/include)
include_directories(/usr/include)

link_directories(/usr/lib)
#target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)

#target_link_libraries(sqlite3)
