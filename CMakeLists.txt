cmake_minimum_required(VERSION 3.5)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(ExternalProject)

ExternalProject_Add(project_zorder
	URL https://codeforge.lbl.gov/frs/download.php/file/418/zorder-lib-24Apr2015.tgz
	PREFIX ${CMAKE_CURRENT_BINARY_DIR}/zorder-lib
	INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/contrib
	PATCH_COMMAND patch -p2 < ${CMAKE_CURRENT_SOURCE_DIR}/zorder-lib.patch
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND make
	INSTALL_COMMAND make install PREFIX=${CMAKE_CURRENT_BINARY_DIR}/contrib
)
ExternalProject_Get_Property(project_zorder INSTALL_DIR)
set(zorder_INSTALL_DIR ${INSTALL_DIR})
add_library(zorder STATIC IMPORTED)
set_property(TARGET zorder PROPERTY IMPORTED_LOCATION ${zorder_INSTALL_DIR}/lib/libzorder.a)
add_dependencies(zorder project_zorder)

# https://ftpmirror.gnu.org/libmicrohttpd/libmicrohttpd-0.9.70.tar.gz
ExternalProject_Add(ProjectMicrohttpd
	URL https://ftpmirror.gnu.org/libmicrohttpd/libmicrohttpd-0.9.70.tar.gz
	PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libmicrohttpd
	INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/contrib
	CONFIGURE_COMMAND ./configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/contrib
	BUILD_IN_SOURCE 1
	BUILD_COMMAND make
	INSTALL_COMMAND make install
)
ExternalProject_Get_Property(ProjectMicrohttpd INSTALL_DIR)
set(ProjectMicrohttpd_INSTALL_DIR ${INSTALL_DIR})
add_library(microhttpd STATIC IMPORTED)
add_dependencies(microhttpd ProjectMicrohttpd)

project(Fg C)
include_directories(${zorder_INSTALL_DIR}/include)
include_directories(${Fg_SOURCE_DIR}/include)
include_directories(${ProjectMicrohttpd_INSTALL_DIR}/include)
link_directories(${zorder_Libtap_INSTALL_DIR}/lib)
link_directories(${ProjectMicrohttpd_INSTALL_DIR}/lib)
link_directories(eglinfo /usr/lib/x86_64-linux-gnu)

add_executable(main main.c src/fg.c)
target_link_libraries(main zorder m)

set(CMAKE_BUILD_TYPE Debug)

add_executable(render src/main_render.c src/render.c src/fg.c src/glad.c)
target_include_directories(render PUBLIC ./include)
target_link_libraries(render zorder m dl GL GLU EGL) 

add_executable(server src/server.c src/render.c src/fg.c src/glad.c)
target_include_directories(server PUBLIC include)
target_link_libraries(server zorder m dl GL GLU EGL z ${CMAKE_CURRENT_BINARY_DIR}/contrib/lib/libmicrohttpd.so)

add_executable(eglinfo src/eglinfo.c)
target_link_libraries(eglinfo PUBLIC dl GL GLU EGL) 
