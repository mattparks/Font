file(GLOB_RECURSE SOURCES_HEADER_FILES
		"*.h"
		"*.hpp"
		)
file(GLOB_RECURSE SOURCES_SOURCE_FILES
		"*.c"
		"*.cpp"
		"*.rc"
		)
add_library(Sources ${SOURCES_HEADER_FILES} ${SOURCES_SOURCE_FILES})
set(SOURCES_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/Sources/")

set_target_properties(Sources PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(Sources PUBLIC SOURCES_EXPORTS)

target_compile_features(Sources PUBLIC cxx_std_17)
target_include_directories(Sources 
		PUBLIC 
		${SOURCES_INCLUDE_DIR} 
	#	PRIVATE
		${FREETYPE_INCLUDE_DIRS}
		)
target_link_libraries(Sources
		PUBLIC
		Vulkan::Vulkan
	#	PRIVATE
		glfw
		${FREETYPE_LIBRARY}
		)
