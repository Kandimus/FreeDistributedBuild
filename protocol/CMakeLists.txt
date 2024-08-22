
cmake_minimum_required (VERSION 3.8)

project(protocol LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Protobu files
set(PROTOBUF_PROTOC_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/bin/protoc.exe")

FILE(GLOB DATAMODEL_PROTOS "*.proto")

FOREACH(proto ${DATAMODEL_PROTOS})
    FILE(TO_NATIVE_PATH ${proto} proto_native)

    #message(STATUS "${PROTOBUF_PROTOC_EXECUTABLE} --proto_path=${CMAKE_CURRENT_SOURCE_DIR} --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${proto_native}")

    EXECUTE_PROCESS (
        COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} --proto_path=${CMAKE_CURRENT_SOURCE_DIR} --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${proto_native}
        RESULT_VARIABLE rv
    )

    # Optional, but that can show the user if something have gone wrong with the proto generation
    IF(${rv})
         MESSAGE("Generation of data model returned ${rv} for proto ${proto}")
    ENDIF()
ENDFOREACH(proto)

# List of files
file(GLOB files_h "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB files_cpp "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB proto_h "${CMAKE_CURRENT_BINARY_DIR}/*.h")
file(GLOB proto_cc "${CMAKE_CURRENT_BINARY_DIR}/*.cc")
file(GLOB net_cpp "${CMAKE_CURRENT_SOURCE_DIR}/../external/smallUtils/net/*.cpp")

file(GLOB utils_cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/tinyxml2/tinyxml2.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/smallUtils/stringex.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/smallUtils/log.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/smallUtils/thread_class.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/smallUtils/win/process.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/smallUtils/tickcount.cpp"
)

# target
add_library (${PROJECT_NAME} STATIC
    ${files_h} ${files_cpp} ${proto_h} ${proto_cc} ${net_cpp} ${utils_cpp})
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

# include
target_include_directories(${PROJECT_NAME} PUBLIC "include")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(${PROJECT_NAME} PUBLIC "../external/smallUtils")

# install
install(FILES
    lib/libprotobuf-lite${BUILD_POSTFIX}.dll
    DESTINATION ${CMAKE_INSTALL_PREFIX})

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    install(FILES lib/debug/abseil_dll.dll DESTINATION ${CMAKE_INSTALL_PREFIX})
else()
    install(FILES lib/abseil_dll.dll DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
