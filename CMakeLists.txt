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

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LINUX TRUE)
endif()

option(DEBUG "Output additional debug information" OFF)
option(SQLITE_STATIC "Link SQLite statically" OFF)

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
if (${MACOSX})
    set_target_properties(json-c PROPERTIES IMPORTED_LOCATION /usr/local/lib/libjson-c.a)
else ()
    # TODO: figure out why json-c is not being statically linked
    set_target_properties(json-c PROPERTIES IMPORTED_LOCATION /usr/lib64/libjson-c.so)
endif (${MACOSX})

print_all_variables()

target_link_libraries(bce PRIVATE sqlite3 json-c curl)

