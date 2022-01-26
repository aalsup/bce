cmake_minimum_required(VERSION 3.21)
project(auto_complete_c C)

set(CMAKE_C_STANDARD 99)

add_executable(auto_complete_c
    main.c
    completion.c
    linked_list.c)

add_subdirectory(test)

include_directories(/usr/local/include)
include_directories(/usr/include)

link_directories(/usr/lib)
target_link_libraries(auto_complete_c sqlite3)

