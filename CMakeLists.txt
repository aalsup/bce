cmake_minimum_required(VERSION 3.21)

# compiler info
project(bce LANGUAGES C VERSION 1.0)
set(CMAKE_C_STANDARD 99)

# version of libraries to link to
set(JSON-C_VERSION 0.15)

# cmake CLI options to alter the build
option(DEBUG "Output additional debug information" OFF)
option(SQLITE_STATIC "Link SQLite statically" OFF)
option(CURL_STATIC "Link cURL statically" OFF)

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
endif ()

target_include_directories(bce PRIVATE /usr/local/include)
target_include_directories(bce PRIVATE /usr/include)

if (${MACOSX})
    target_link_directories(bce PRIVATE /usr/local/lib)
else ()
    target_link_directories(bce PRIVATE /usr/lib640)
endif ()

# Specifying '/usr/local/lib' strangely doesn't work on MacOS. Use 'find_library' instead.
# see: https://stackoverflow.com/questions/52679871/cannot-find-library-under-usr-local-lib

# JSON-C: static link
find_package(json-c REQUIRED CONFIG)
add_library(json-c STATIC IMPORTED)
if (${MACOSX})
    set_target_properties(json-c PROPERTIES IMPORTED_LOCATION /usr/local/lib/libjson-c.a)
else ()
    # TODO: figure out why json-c is not being statically linked.
    # https://stackoverflow.com/questions/50783730/adding-curl-as-static-library-to-a-c-cmake-project
    set_target_properties(json-c PROPERTIES IMPORTED_LOCATION /usr/lib64/libjson-c.so)
endif ()
target_link_libraries(bce PRIVATE json-c)

# CURL: dynamic vs static link
if (${CURL_STATIC})
    find_package(CURL REQUIRED)
    target_include_directories(bce PRIVATE ${CURL_INCLUDE_DIR})
    target_link_libraries(bce PRIVATE ${CURL_LIBRARIES})
else ()
    target_link_libraries(bce PRIVATE curl)
endif ()

# SQLite: dynamic link
target_link_libraries(bce PRIVATE sqlite3)

# debug stuff
macro (print_all_variables)
    message(STATUS "print_all_variables------------------------------------------{")
    get_cmake_property(_variableNames VARIABLES)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
    message(STATUS "print_all_variables------------------------------------------}")
endmacro ()

print_all_variables()


