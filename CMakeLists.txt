# fldot - Flow to DOT converter
# KaisarCode - CMake build configuration
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.14)
project(fldot C)

set(CMAKE_C_STANDARD 11)

if(NOT DEFINED FLDOT_BUILD_VERSION)
    set(FLDOT_BUILD_VERSION 0)
endif()

option(FLDOT_NATIVE "Enable native hardware optimizations" OFF)
option(FLDOT_BUILD_TESTS "Build fldot tests" OFF)

if(NOT MSVC)
    add_compile_options(-O3 -Wall -Wextra -Werror)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        add_compile_options(-Wno-unused-command-line-argument)
    endif()
    if(FLDOT_NATIVE)
        add_compile_options(-march=native)
    endif()
    if(NOT WIN32)
        add_definitions(-D_GNU_SOURCE)
    endif()
else()
    add_compile_options(/O2 /W3 /WX)
endif()

set_source_files_properties(src/fldot.c PROPERTIES
    COMPILE_DEFINITIONS KC_FLDOT_BUILD_VERSION=${FLDOT_BUILD_VERSION}
)

add_executable(fldot
    src/fldot.c
    src/theme.h
)
target_include_directories(fldot PRIVATE src)

if(UNIX AND NOT ANDROID AND NOT APPLE)
    target_link_libraries(fldot pthread m)
endif()

if(FLDOT_BUILD_TESTS)
    enable_testing()

    add_executable(fldot_contract_test src/test.c)
    target_include_directories(fldot_contract_test PRIVATE src)

    if(FLDOT_TEST_CLI)
        target_compile_definitions(fldot_contract_test PRIVATE
            KC_FLDOT_TEST_CLI="${FLDOT_TEST_CLI}"
        )
    else()
        target_compile_definitions(fldot_contract_test PRIVATE
            KC_FLDOT_TEST_CLI="$<TARGET_FILE:fldot>"
        )
    endif()

    add_test(NAME cli-help COMMAND fldot_contract_test help)
    add_test(NAME cli-version COMMAND fldot_contract_test version)
    add_test(NAME cli-basic-output COMMAND fldot_contract_test basic-output)
    add_test(NAME cli-explicit-output COMMAND fldot_contract_test explicit-output)
    add_test(NAME cli-meta-titles COMMAND fldot_contract_test meta-titles)
endif()
