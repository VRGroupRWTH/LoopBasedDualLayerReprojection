cmake_minimum_required(VERSION 3.6)

project(shared)

set(SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/source/)

file(GLOB_RECURSE SOURCE_FILES ${SOURCE_DIRECTORY}*.cpp)
file(GLOB_RECURSE HEADER_FILES ${SOURCE_DIRECTORY}*.hpp)

add_library(shared ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(shared PUBLIC ${SOURCE_DIRECTORY})

if(MSVC)
    source_group("Source" REGULAR_EXPRESSION ${SOURCE_DIRECTORY}*)
endif()