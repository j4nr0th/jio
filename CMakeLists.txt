cmake_minimum_required(VERSION 3.25)
project(jio C)

set(CMAKE_C_STANDARD 99)



list(APPEND JIO_SOURCE_FILES source/iobase.c source/iocfg.c source/ioerr.c source/iocsv.c source/ioxml.c)
list(APPEND JIO_HEADER_FILES include/jio/iobase.h include/jio/iocfg.h include/jio/ioerr.h include/jio/iocsv.h include/jio/ioxml.h)

add_library(jio ${JIO_SOURCE_FILES} ${JIO_HEADER_FILES})
target_link_libraries(jio PRIVATE jmem)
enable_testing()
add_subdirectory(source/tests)
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(jio PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif ()
target_sources(jio PUBLIC FILE_SET jio_header_list TYPE HEADERS BASE_DIRS include/jio FILES ${JIO_HEADER_FILES})
include(GNUInstallDirs)
install(TARGETS jio
        FILE_SET jio_header_list
        DESTINATION "/usr/include/jio"
        ARCHIVE
        DESTINATION "/usr/lib")
