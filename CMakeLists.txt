cmake_minimum_required(VERSION 3.21)
project(bash_complete_extension LANGUAGES C VERSION 1.0)
set(CMAKE_C_STANDARD 99)

set(JSON-C_VERSION 0.15)

option(DEBUG "Output additional debug information" OFF)

add_executable(bce
        main.c
        dbutil.h dbutil.c
        data_model.h data_model.c
        linked_list.h linked_list.c
        input.h input.c
        download.h download.c
        error.h error.c
        prune.h prune.c
        cli.h cli.c
        uuid4.h uuid4.c)

add_subdirectory(test)

macro(print_all_variables)
    message(STATUS "print_all_variables------------------------------------------{")
    get_cmake_property(_variableNames VARIABLES)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
    message(STATUS "print_all_variables------------------------------------------}")
endmacro()

target_include_directories(bce PRIVATE /usr/local/include)
target_include_directories(bce PRIVATE /usr/include)

# Specifying '/usr/local/lib' strangely doesn't work on MacOS. Use 'find_library' instead.
# see: https://stackoverflow.com/questions/52679871/cannot-find-library-under-usr-local-lib

# static link json-c library within the application
find_package(json-c REQUIRED CONFIG)
add_library(json-c STATIC IMPORTED)
set_target_properties(json-c PROPERTIES IMPORTED_LOCATION /usr/local/Cellar/json-c/${JSON-C_VERSION}/lib/libjson-c.a)

print_all_variables()

target_link_libraries(bce PRIVATE sqlite3 json-c curl)

