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
option(SQLITE_STATIC "Link SQLite statically" OFF)

target_include_directories(bce PRIVATE /usr/local/include)
target_include_directories(bce PRIVATE /usr/include)
#include_directories(/usr/local/include)
#include_directories(/usr/include)

# Specifying '/usr/local/lib' strangely doesn't work on MacOS. Use 'find_library' instead.
# see: https://stackoverflow.com/questions/52679871/cannot-find-library-under-usr-local-lib
# link_directories(/usr/local/lib)
# find_library(JSONC libjson-c.dylib)
target_link_directories(bce PRIVATE /usr/lib)
#link_directories(/usr/lib)

# bundle json-c library within the application
find_package(json-c CONFIG)
add_library(json-c STATIC IMPORTED)
set_target_properties(json-c PROPERTIES IMPORTED_LOCATION /usr/local/lib/libjson-c.a)

if (SQLITE_STATIC)
    # bundle sqlite3 library within the application
    link_directories(bce PRIVATE /usr/local/Cellar/sqlite/3.37.2/lib)
    find_package(sqlite3 CONFIG)
    add_library(sqlite3 STATIC IMPORTED)
    set_target_properties(sqlite3 PROPERTIES IMPORTED_LOCATION /usr/local/Cellar/sqlite/3.37.2/lib/libsqlite3.a)
else ()
    # allow sqlite3 to be dynamically linked at runtime
endif (SQLITE_STATIC)

target_link_libraries(bce PRIVATE sqlite3 json-c)

