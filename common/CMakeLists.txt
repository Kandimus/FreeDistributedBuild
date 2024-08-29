cmake_minimum_required (VERSION 3.8)

project("common" LANGUAGES CXX)

# target
add_library (${PROJECT_NAME} STATIC
    "project.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/smallUtils/log.cpp"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

# includes
target_include_directories(${PROJECT_NAME} PRIVATE "../external/tinyxml2")
target_include_directories(${PROJECT_NAME} PRIVATE "../external/smallUtils")
