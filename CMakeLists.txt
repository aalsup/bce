cmake_minimum_required(VERSION 3.21)
project(auto_complete_c C)

set(CMAKE_C_STANDARD 99)

add_executable(auto_complete_c
        main.c
        completion_model.h completion_model.c
        linked_list.h linked_list.c
        completion_input.h completion_input.c
        error.h
        prune.h prune.c
        uuid4.h uuid4.c)

add_subdirectory(test)

include_directories(/usr/local/include)
include_directories(/usr/include)

link_directories(/usr/lib)
target_link_libraries(auto_complete_c sqlite3)

