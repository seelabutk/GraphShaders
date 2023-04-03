
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

install(
    PROGRAMS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/GraphShaderTranspiler.py"
    TYPE
        BIN
)