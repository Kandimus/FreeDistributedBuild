# CMakeList.txt : CMake project for FreeIncrediBuildsConsole, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project(FreeDistributedBuild LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable (${PROJECT_NAME}
    "../external/tinyxml2/tinyxml2.cpp"
    "../external/smallUtils/stringex.cpp"
    "../external/smallUtils/log.cpp"
    "Process.cpp"
    "FreeDistributedBuild.cpp"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

target_include_directories(${PROJECT_NAME} PRIVATE "../external/tinyxml2")
target_include_directories(${PROJECT_NAME} PRIVATE "../external/smallUtils")

# TODO: Add tests and install targets if needed.
