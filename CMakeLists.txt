cmake_minimum_required(VERSION 3.21)
project(bash_complete_extension LANGUAGES C VERSION 1.0)
set(CMAKE_C_STANDARD 99)

add_executable(bash_complete_extension
        main.c
        dbutil.h dbutil.c
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
target_link_libraries(bash_complete_extension sqlite3)

