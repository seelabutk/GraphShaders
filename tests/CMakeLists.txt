cmake_minimum_required(VERSION 3.16)

include(ExternalProject)

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


project(FgTest C)
add_library(common tests/common.c)
add_executable(test_fg tests/test_fg.c)
target_link_libraries(test_fg fg zorder m libtap)
