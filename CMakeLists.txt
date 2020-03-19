cmake_minimum_required(VERSION 2.6)

option(ENABLE_EXAMPLES "Build nanio example programs" OFF)
option(ENABLE_TESTS    "Build nanio tests programs" OFF)

include_directories(include/)

add_subdirectory(library)
add_subdirectory(include)

if(ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()

if(ENABLE_TESTS)
    add_subdirectory(tests)
endif()
