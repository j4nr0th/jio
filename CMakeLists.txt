cmake_minimum_required(VERSION 3.25)
project(jio C)

set(CMAKE_C_STANDARD 99)



list(APPEND JIO_SOURCE_FILES source/iobase.c source/iocfg.c source/ioerr.c source/iocsv.c source/ioxml.c source/internal.c)
list(APPEND JIO_HEADER_FILES include/jio/iobase.h include/jio/iocfg.h include/jio/ioerr.h include/jio/iocsv.h include/jio/ioxml.h)

add_library(jio ${JIO_SOURCE_FILES} ${JIO_HEADER_FILES}
        source/internal.h)
enable_testing()
add_subdirectory(source/tests)
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(jio PRIVATE -Wall -Wextra -Werror)
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_definitions(jio PRIVATE _CRT_SECURE_NO_WARNINGS)
endif ()
target_sources(jio PUBLIC FILE_SET jio_header_list TYPE HEADERS BASE_DIRS include/jio FILES ${JIO_HEADER_FILES})

