cmake_minimum_required(VERSION 3.14)
project(fldot C)

set(CMAKE_C_STANDARD 11)


option(BIN_NATIVE "Enable native hardware optimizations" OFF)

if(NOT MSVC)
    add_compile_options(-O3 -Wall -Wextra -Werror)
    if(BIN_NATIVE)
        add_compile_options(-march=native)
    endif()
    add_definitions(-D_GNU_SOURCE)
else()
    add_compile_options(/O2 /W3 /WX)
endif()

# Main executable
add_executable(fldot
    src/fldot.c
)

target_include_directories(fldot PRIVATE src)

if(UNIX AND NOT ANDROID)
    target_link_libraries(fldot pthread m)
endif()
