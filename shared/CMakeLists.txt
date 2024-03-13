cmake_minimum_required(VERSION 3.27)
project(interactive-3d-streaming)

include(cmake/dependencies.cmake)

### protocol lib ###
add_library(
    i3dsprotocol
    protocol/protocol.hpp
    protocol/protocol.cpp
	protocol/huffman.hpp
	protocol/huffman.cpp
	protocol/geometry_codec.hpp
    protocol/geometry_codec.cpp
)
add_library(i3ds::protocol ALIAS i3dsprotocol)
target_include_directories(
    i3dsprotocol
    PUBLIC
        protocol
)
target_compile_features(
    i3dsprotocol 
    PUBLIC 
        cxx_std_20
)

### client lib ###
add_library(
    i3dsclient
    client/session_client.hpp
    client/session_client.cpp
)
add_library(i3ds::client ALIAS i3dsclient)
target_include_directories(
    i3dsclient
    PUBLIC
        client
)
target_link_libraries(i3dsclient
    PUBLIC
        i3ds::protocol
        fmt::fmt
        spdlog::spdlog
)
if (NOT ${CMAKE_CXX_COMPILER} MATCHES "emscripten")
    target_link_libraries(i3dsclient
        PUBLIC
            ixwebsocket
    )
else ()
    target_compile_definitions(i3dsclient
        PUBLIC
            I3DS_EMSCRIPTEN=1
    )
    target_link_libraries(i3dsclient
        PUBLIC
            websocket.js
    )
endif ()

if (NOT ${CMAKE_CXX_COMPILER} MATCHES "emscripten")
    ### server lib ###
    add_library(
        i3dsserver
        server/streaming_server.hpp
        server/streaming_server.cpp
    )
    add_library(i3ds::server ALIAS i3dsserver)
    target_include_directories(
        i3dsserver
        PUBLIC
            server
    )
    target_link_libraries(i3dsserver
        PUBLIC
            uWebSockets
            i3ds::protocol
            spdlog::spdlog
            jsoncons
    )

    # ### example app ###
    # add_executable(
    #     i3ds-example
    #     example/example.cpp
    # )

    # target_link_libraries(
    #     i3ds-example
    #     PRIVATE
    #         i3ds::server
    #         i3ds::client
    # )

    # ### example sever app ###
    # add_executable(
    #     i3ds-example-server
    #     example/example_server.cpp
    # )

    # target_link_libraries(
    #     i3ds-example-server
    #     PRIVATE
    #         i3ds::server
    # )

    # ### example client app ###
    # add_executable(
    #     i3ds-example-client
    #     example/example_client.cpp
    # )

    # target_link_libraries(
    #     i3ds-example-client
    #     PRIVATE
    #         i3ds::client
    # )
endif ()
