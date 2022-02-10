cmake_minimum_required(VERSION 3.21)
project(bash_complete_extension LANGUAGES C VERSION 1.0)
set(CMAKE_C_STANDARD 99)

add_executable(bce
        main.c
        dbutil.h dbutil.c
        data_model.h data_model.c
        linked_list.h linked_list.c
        input.h input.c
        error.h
        prune.h prune.c
        cli.h cli.c
        uuid4.h uuid4.c)

add_subdirectory(test)

option(DEBUG "Output additional debug information" OFF)

include_directories(/usr/local/include)
include_directories(/usr/include)

link_directories(/usr/lib)
target_link_libraries(bce sqlite3)

