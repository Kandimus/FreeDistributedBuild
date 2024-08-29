cmake_minimum_required (VERSION 3.8)

project(fdbdaemon LANGUAGES CXX)

# target
add_executable (${PROJECT_NAME}
    "daemon.cpp"
    "tcp_protobufclient.cpp"
    "udp_daemonserver.cpp"
    "window.cpp"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

# includes
target_include_directories(${PROJECT_NAME} PRIVATE "../external/tinyxml2")
target_include_directories(${PROJECT_NAME} PRIVATE "../external/smallUtils")
target_include_directories(${PROJECT_NAME} PRIVATE "../protocol")
target_include_directories(${PROJECT_NAME} PRIVATE "../common")

# libraries
target_link_directories(${PROJECT_NAME} PUBLIC "../protocol/lib")

target_link_libraries(${PROJECT_NAME} PRIVATE "Ws2_32")
target_link_libraries(${PROJECT_NAME} PRIVATE "common")
target_link_libraries(${PROJECT_NAME} PRIVATE "protocol")
target_link_libraries(${PROJECT_NAME} PRIVATE "libprotobuf-lite${BUILD_POSTFIX}")
target_link_libraries(${PROJECT_NAME} PRIVATE "abseil_dll${BUILD_POSTFIX}")

# install
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})
