cmake_minimum_required(VERSION 3.16)
project(AncestryTree C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(ANCESTRYTREE_BUILD_TESTS "Build unit tests" ON)

set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/*.c)

add_library(ancestrytree_lib STATIC ${PROJECT_SOURCES})

if(MSVC)
    target_compile_options(ancestrytree_lib PRIVATE /W4 /permissive- /Zc:preprocessor)
else()
    target_compile_options(ancestrytree_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(ancestrytree_lib PUBLIC ${PROJECT_INCLUDE_DIR})

find_package(raylib QUIET)
if(NOT raylib_FOUND)
    message(STATUS "raylib not found via find_package; trying manual hints")
    if(DEFINED ENV{RAYLIB_HOME})
        set(RAYLIB_SEARCH_PATH $ENV{RAYLIB_HOME})
        find_library(RAYLIB_LIB raylib HINTS ${RAYLIB_SEARCH_PATH}/lib ${RAYLIB_SEARCH_PATH}/lib64)
        find_path(RAYLIB_INCLUDE raylib.h HINTS ${RAYLIB_SEARCH_PATH}/include)
        if(RAYLIB_LIB AND RAYLIB_INCLUDE)
            add_library(raylib::raylib UNKNOWN IMPORTED)
            set_target_properties(raylib::raylib PROPERTIES
                IMPORTED_LOCATION ${RAYLIB_LIB}
                INTERFACE_INCLUDE_DIRECTORIES ${RAYLIB_INCLUDE})
            set(raylib_FOUND ON)
        endif()
    endif()
endif()

if(NOT raylib_FOUND)
    message(WARNING "raylib was not located. Configure RAYLIB_HOME or install raylib to satisfy linkage.")
endif()

add_executable(ancestrytree src/main.c)

target_link_libraries(ancestrytree PRIVATE ancestrytree_lib)

if(raylib_FOUND)
    target_link_libraries(ancestrytree PRIVATE raylib::raylib)
endif()

if(WIN32)
    target_link_libraries(ancestrytree PRIVATE opengl32 gdi32 winmm)
elseif(APPLE)
    target_link_libraries(ancestrytree PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework OpenGL"
        "-framework CoreAudio")
else()
    target_link_libraries(ancestrytree PRIVATE m pthread dl rt X11)
endif()

if(ANCESTRYTREE_BUILD_TESTS)
    enable_testing()
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.c)
    add_executable(ancestrytree_tests ${TEST_SOURCES})
    target_link_libraries(ancestrytree_tests PRIVATE ancestrytree_lib)
    if(raylib_FOUND)
        target_link_libraries(ancestrytree_tests PRIVATE raylib::raylib)
    endif()
    if(WIN32)
        target_link_libraries(ancestrytree_tests PRIVATE opengl32 gdi32 winmm)
    elseif(APPLE)
        target_link_libraries(ancestrytree_tests PRIVATE
            "-framework Cocoa"
            "-framework IOKit"
            "-framework CoreVideo"
            "-framework OpenGL"
            "-framework CoreAudio")
    else()
        target_link_libraries(ancestrytree_tests PRIVATE m pthread dl rt X11)
    endif()
    add_test(NAME ancestrytree_tests COMMAND ancestrytree_tests)
endif()
