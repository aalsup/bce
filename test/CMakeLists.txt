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
        completion_input_tests.cpp
        completion_model_tests.cpp
        download_tests.cpp
        ../linked_list.c ../linked_list.h
        ../dbutil.c ../dbutil.h
        ../input.c ../input.h
        ../download.c ../download.h
        ../data_model.c ../data_model.h
        ../error.h
        ../prune.c ../prune.h
)

set_target_properties(tests PROPERTIES LINKER_LANGUAGE CXX)

include_directories(/usr/local/include)
include_directories(/usr/include)

link_directories(/usr/lib)

target_link_libraries(tests sqlite3 curl)

