cmake_minimum_required (VERSION 3.8)

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(BUILD_POSTFIX "d")
else()
    set(BUILD_POSTFIX "")
endif()

message(STATUS "mode: ${CMAKE_BUILD_TYPE}")
message(STATUS "posfix: ${BUILD_POSTFIX}")

add_subdirectory(protocol)
add_subdirectory(console)
add_subdirectory(daemon)

project(FreeDistributedBuild LANGUAGES CXX)
