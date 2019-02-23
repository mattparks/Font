# 3.8.0 required for cxx_std_17
cmake_minimum_required(VERSION 3.8.0 FATAL_ERROR)
project(Font VERSION 0.0.3 LANGUAGES C CXX)

# Sets the install directories defined by GNU
include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

# Use ccache if it is installed, will cach previous compilations
find_program(CCACHE_FOUND "ccache")
if(CCACHE_FOUND)
	message(STATUS "Using ccache for building.")
	set_property(GLOBAL PROPERTY LAUNCH_COMPILE ccache)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

# Dependencies
set(BUILD_TESTS OFF CACHE INTERNAL "Build test applications")
set(ACID_INSTALL_EXAMPLES OFF CACHE INTERNAL "Installs the examples")
set(ACID_INSTALL_RESOURCES OFF CACHE INTERNAL "Installs the Resources directory")
add_subdirectory(${PROJECT_SOURCE_DIR}/Libraries/Acid)

# Sources
add_subdirectory(Sources)
