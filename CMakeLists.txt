
#--- Main


project(GraphShader)
cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_LIST_DIR}/cmake"
)


#--- Main

add_executable(GraphShaderEngine
    src/GraphShaderEngine.cpp
    src/glad.c
    src/glad_egl.c
)

target_compile_definitions(GraphShaderEngine
    PRIVATE
        _GS_MAIN_
)

target_include_directories(GraphShaderEngine
    PUBLIC
        src
)

target_link_libraries(GraphShaderEngine
    PUBLIC
        -lEGL
        -ldl
)

install(
    TARGETS
        GraphShaderEngine
)


add_library(graphshader
    SHARED
        src/GraphShaderEngine.cpp
        src/glad.c
        src/glad_egl.c
)

target_include_directories(graphshader
    PUBLIC
        src
)

target_link_libraries(graphshader
    PUBLIC
        -lEGL
        -ldl
)

install(
    TARGETS
        graphshader
)


install(
    PROGRAMS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/GraphShaderTranspiler.py"
    TYPE
        BIN
)