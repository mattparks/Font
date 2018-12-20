cmake_minimum_required(VERSION 3.8.0 FATAL_ERROR)
project(Fonts VERSION 0.1.0 LANGUAGES C CXX)

option(BUILD_SHARED_LIBS "Build Shared Libraries" ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "Adds a postfix for debug-built libraries.")
if(WIN32)
	set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()

include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

find_package(Vulkan REQUIRED)

foreach(_glfw3_option "GLFW_BUILD_TESTS" "GLFW_BUILD_EXAMPLES" "GLFW_BUILD_DOCS" "GLFW_INSTALL" "GLFW_VULKAN_STATIC")
	set(${_glfw3_option} OFF CACHE INTERNAL "")
endforeach()

add_subdirectory(${PROJECT_SOURCE_DIR}/Libraries/glfw)

foreach(_freetype_option "CMAKE_DISABLE_FIND_PACKAGE_HarfBuzz")
	set(${_freetype_option} TRUE CACHE INTERNAL "")
endforeach()

add_subdirectory(${PROJECT_SOURCE_DIR}/Libraries/freetype2)
set(FREETYPE_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/Libraries/freetype2/include")
set(FREETYPE_LIBRARY "freetype")
	
add_subdirectory(Sources)
add_subdirectory(Test)
