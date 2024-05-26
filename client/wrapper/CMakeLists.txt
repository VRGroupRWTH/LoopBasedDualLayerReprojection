cmake_minimum_required(VERSION 3.6)

project(wrapper)

set(SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/source/)
set(BINARY_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/binary/)
set(SHARED_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/)

file(GLOB_RECURSE SOURCE_FILES ${SOURCE_DIRECTORY}*.cpp)
file(GLOB_RECURSE HEADER_FILES ${SOURCE_DIRECTORY}*.hpp)

# General Settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "--emit-tsd wrapper.d.ts -O3") 

add_subdirectory(${SHARED_DIRECTORY} ${CMAKE_CURRENT_BINARY_DIR}/shared)

add_executable(wrapper ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(wrapper shared)
target_link_libraries(wrapper embind)

target_link_options(wrapper PRIVATE -sENVIRONMENT=web,worker)
target_link_options(wrapper PRIVATE -sMODULARIZE=1)
target_link_options(wrapper PRIVATE -sEXPORT_NAME=wrapper)
target_link_options(wrapper PRIVATE -sEXPORT_ES6=1)
target_link_options(wrapper PRIVATE --no-entry)

add_custom_command(
    TARGET wrapper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BINARY_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/wrapper.js ${BINARY_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/wrapper.wasm ${BINARY_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/wrapper.d.ts ${BINARY_DIRECTORY}
)