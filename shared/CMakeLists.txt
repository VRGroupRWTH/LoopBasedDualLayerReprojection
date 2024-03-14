cmake_minimum_required(VERSION 3.6)

project(shared)

set(SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/source/)
set(EXTERN_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/extern/)
set(U_WEB_SOCKETS_DIRECTORY ${EXTERN_DIRECTORY}/uWebSockets)
set(U_SOCKETS_DIRECTORY ${U_WEB_SOCKETS_DIRECTORY}/uSockets)
set(LIBUV_DIRECTORY ${EXTERN_DIRECTORY}/libuv)

file(GLOB_RECURSE SOURCE_FILES ${SOURCE_DIRECTORY}*.cpp)
file(GLOB_RECURSE HEADER_FILES ${SOURCE_DIRECTORY}*.hpp)

file(GLOB_RECURSE U_WEB_SOCKETS_HEADER_FILES ${U_WEB_SOCKETS_DIRECTORY}/src/*.h)
file(GLOB_RECURSE U_SOCKETS_SOURCE_FILES ${U_SOCKETS_DIRECTORY}/src/*.c ${U_SOCKETS_DIRECTORY}/src/*.cpp)
file(GLOB_RECURSE U_SOCKETS_HEADER_FILES ${U_SOCKETS_DIRECTORY}/src/*.h)

add_subdirectory(${LIBUV_DIRECTORY})

################################################################
# uWebSockets

add_library(uWebSockets ${U_WEB_SOCKETS_HEADER_FILES} ${U_SOCKETS_SOURCE_FILES} ${U_SOCKETS_HEADER_FILES})

target_compile_definitions(uWebSockets PUBLIC LIBUS_USE_LIBUV)
target_compile_definitions(uWebSockets PUBLIC LIBUS_NO_SSL)
target_compile_definitions(uWebSockets PUBLIC UWS_NO_ZLIB)

target_link_libraries(uWebSockets PUBLIC uv_a)
target_include_directories(uWebSockets PUBLIC ${U_WEB_SOCKETS_DIRECTORY}/src)
target_include_directories(uWebSockets PUBLIC ${U_SOCKETS_DIRECTORY}/src)

################################################################
# Client

set(CLIENT_SOURCE_FILES ${SOURCE_FILES})
set(CLIENT_SOURCE_HEADER ${HEADER_FILES})
list(REMOVE_ITEM CLIENT_SOURCE_FILES ${SOURCE_DIRECTORY}stream_server.cpp)
list(REMOVE_ITEM CLIENT_SOURCE_HEADER ${SOURCE_DIRECTORY}stream_server.hpp)

add_library(stream_client ${CLIENT_SOURCE_FILES} ${CLIENT_SOURCE_HEADER})

target_compile_features(stream_client PUBLIC cxx_std_20)
target_compile_definitions(stream_client PUBLIC I3DS_EMSCRIPTEN = 1)

target_link_libraries(stream_client PUBLIC websocket.js)
target_include_directories(stream_client PUBLIC ${SOURCE_DIRECTORY})

################################################################
# Server

set(SERVER_SOURCE_FILES ${SOURCE_FILES})
set(SERVER_SOURCE_HEADER ${HEADER_FILES})
list(REMOVE_ITEM SERVER_SOURCE_FILES ${SOURCE_DIRECTORY}stream_client.cpp)
list(REMOVE_ITEM SERVER_SOURCE_HEADER ${SOURCE_DIRECTORY}stream_client.hpp)

add_library(stream_server ${SERVER_SOURCE_FILES} ${SERVER_SOURCE_HEADER})

target_compile_features(stream_server PUBLIC cxx_std_20)

target_link_libraries(stream_server PUBLIC uWebSockets)
target_include_directories(stream_server PUBLIC ${SOURCE_DIRECTORY})