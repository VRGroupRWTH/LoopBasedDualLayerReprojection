cmake_minimum_required(VERSION 3.27)
project(interactive-3d-streaming-client)

include(cmake/CPM.cmake)
add_subdirectory(interactive-3d-streaming)

add_executable(
    ar-streaming-lib
    src/lib.cpp
)

target_link_libraries(
    ar-streaming-lib
    PRIVATE
        embind
        i3ds::protocol
)

target_link_options(
    ar-streaming-lib
    PRIVATE
        -sENVIRONMENT=web,worker
        -sMODULARIZE=1
        -sEXPORT_NAME=createARStreamingLib
        -sEXPORT_ES6=1
        # -sASSERTIONS
        # -sALLOW_MEMORY_GROWTH
        --no-entry
)

add_custom_target(
    copy-type-definitions ALL
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/ar-streaming-lib.d.ts ${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(ar-streaming-lib copy-type-definitions)

# add

# add_executable(
#     client
#     src/main.cpp
#     src/app.hpp src/app.cpp
#     src/decoder.hpp src/decoder.cpp
#     src/layer.hpp src/layer.cpp
#     src/shader_helper.hpp src/shader_helper.cpp
#     src/log_sink.hpp src/log_sink.cpp
#     src/study.hpp src/study.cpp
# )
# target_compile_features(
#     client 
#     PRIVATE 
#         cxx_std_20
# )
# target_link_libraries(
#     client
#     PRIVATE
#         i3ds::protocol
#         i3ds::client
#         glm::glm
#         imgui::imgui
#         imgui::sdl2
#         imgui::gles3
#         glm::glm
#         embind
#         opencv_core
# )

# if (${CMAKE_CXX_COMPILER} MATCHES "emscripten")
#     # add_custom_command(
#     #     OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/index.html
#     #     COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/index.html ${CMAKE_CURRENT_BINARY_DIR}
#     #     MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/index.html
#     # )
#     add_custom_target(
#         copy-index-html ALL
#         COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/index.html ${CMAKE_CURRENT_BINARY_DIR}
#     )
#     add_dependencies(client copy-index-html)
#     add_custom_target(
#         copy-test-html ALL
#         COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test.html ${CMAKE_CURRENT_BINARY_DIR}
#     )
#     add_dependencies(client copy-test-html)

#     target_link_libraries(
#         client
#         PRIVATE
#             webxr
#     )
# endif ()





set(CLIENT_SOURCE_FILES ${SOURCE_FILES})
set(CLIENT_SOURCE_HEADER ${HEADER_FILES})
list(REMOVE_ITEM CLIENT_SOURCE_FILES ${SOURCE_DIRECTORY}stream_server.cpp)
list(REMOVE_ITEM CLIENT_SOURCE_HEADER ${SOURCE_DIRECTORY}stream_server.hpp)

add_library(stream_client ${CLIENT_SOURCE_FILES} ${CLIENT_SOURCE_HEADER})

target_compile_features(stream_client PUBLIC cxx_std_20)
target_compile_definitions(stream_client PUBLIC I3DS_EMSCRIPTEN = 1)

target_link_libraries(stream_client PUBLIC websocket.js)
target_include_directories(stream_client PUBLIC ${SOURCE_DIRECTORY})