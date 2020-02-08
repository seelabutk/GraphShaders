cmake_minimum_required(VERSION 3.16)

include(ExternalProject)

ExternalProject_Add(project_zorder
	URL https://codeforge.lbl.gov/frs/download.php/file/418/zorder-lib-24Apr2015.tgz
	PREFIX ${CMAKE_CURRENT_BINARY_DIR}/zorder-lib
	PATCH_COMMAND patch -p2 < ${Fg_SOURCE_DIR}/zorder-lib.patch
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND make
	INSTALL_COMMAND make install
	PREFIX=${CMAKE_CURRENT_BINARY_DIR}/zorder-lib
)
ExternalProject_Get_Property(project_zorder INSTALL_DIR)
add_library(zorder STATIC IMPORTED)
set_property(TARGET zorder PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libzorder.a)
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
add_library(libtap STATIC IMPORTED)
set_property(TARGET libtap PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libtap.a)
add_dependencies(libtap Libtap)

project(Fg C)
add_executable(main src/main.c src/fg.c)
include_directories(${INSTALL_DIR}/include)
include_directories(${Fg_SOURCE_DIR}/include)
target_link_libraries(main zorder m)

add_executable(test tests/test_fg.c)
target_link_libraries(test zorder m libtap)
