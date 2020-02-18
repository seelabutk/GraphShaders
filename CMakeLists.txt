cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

ExternalProject_Add(project_zorder
	URL https://codeforge.lbl.gov/frs/download.php/file/418/zorder-lib-24Apr2015.tgz
	PREFIX ${CMAKE_CURRENT_BINARY_DIR}/zorder-lib
	PATCH_COMMAND patch -p2 < ${CMAKE_CURRENT_SOURCE_DIR}/zorder-lib.patch
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND make
	INSTALL_COMMAND make install
	PREFIX=${CMAKE_CURRENT_BINARY_DIR}/zorder-lib
)
ExternalProject_Get_Property(project_zorder INSTALL_DIR)
set(zorder_INSTALL_DIR ${INSTALL_DIR})
add_library(zorder STATIC IMPORTED)
set_property(TARGET zorder PROPERTY IMPORTED_LOCATION ${zorder_INSTALL_DIR}/lib/libzorder.a)
add_dependencies(zorder project_zorder)

ExternalProject_Add(Libtap
	GIT_REPOSITORY https://github.com/zorgnax/libtap.git
	GIT_TAG 14e6708db5215a657e615171682a1741023c9c0c
	PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libtap
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND make
	INSTALL_COMMAND make install PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libtap
)
ExternalProject_Get_Property(Libtap INSTALL_DIR)
set(Libtap_INSTALL_DIR ${INSTALL_DIR})
add_library(libtap STATIC IMPORTED)
set_property(TARGET libtap PROPERTY IMPORTED_LOCATION ${Libtap_INSTALL_DIR}/lib/libtap.a)
add_dependencies(libtap Libtap)

project(Fg C)
include_directories(${zorder_INSTALL_DIR}/include)
include_directories(${Libtap_INSTALL_DIR}/include)
include_directories(${Fg_SOURCE_DIR}/include)
link_directories(${zorder_Libtap_INSTALL_DIR}/lib)
link_directories(${Libtap_INSTALL_DIR}/lib)

add_executable(main src/main.c src/fg.c)
target_link_libraries(main zorder m)

add_executable(test_fg tests/test_fg.c tests/common.o src/fg.c)
target_link_libraries(test_fg zorder m tap)

set(CMAKE_BUILD_TYPE Debug)

add_executable(render src/render.c src/fg.c src/glad.c)
target_include_directories(render PUBLIC ./include)
target_link_libraries(render zorder m tap dl GL GLU glut) 
