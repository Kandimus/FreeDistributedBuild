cmake_minimum_required (VERSION 3.8)

project(fdbconsole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# target
add_executable (${PROJECT_NAME}
    "tcp_protobufserver.cpp"
    "console.cpp"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

# inludes
target_include_directories(${PROJECT_NAME} PRIVATE "../external/tinyxml2")
target_include_directories(${PROJECT_NAME} PRIVATE "../external/smallUtils")
target_include_directories(${PROJECT_NAME} PRIVATE "../protocol")

# libraries
target_link_directories(${PROJECT_NAME} PUBLIC "../protocol/lib")

target_link_libraries(${PROJECT_NAME} PRIVATE "Ws2_32")
target_link_libraries(${PROJECT_NAME} PRIVATE "protocol")
target_link_libraries(${PROJECT_NAME} PRIVATE "libprotobuf-lite${BUILD_POSTFIX}")
target_link_libraries(${PROJECT_NAME} PRIVATE "abseil_dll${BUILD_POSTFIX}")

# install
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES
    task.xml
    DESTINATION ${CMAKE_INSTALL_PREFIX})

