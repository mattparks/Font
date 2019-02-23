file(GLOB_RECURSE FONT_HEADER_FILES
	"*.h"
	"*.hpp"
	)
file(GLOB_RECURSE FONT_SOURCE_FILES
	"*.c"
	"*.cpp"
	"*.rc"
	)
set(FONT_SOURCES
	${FONT_HEADER_FILES}
	${FONT_SOURCE_FILES}
	)
set(FONT_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/Sources/")

find_package(Freetype QUIET)
if(NOT FREETYPE_FOUND)
	set(_ACID_ALL_SYSTEM_LIBS false)
	foreach(_freetype_option "CMAKE_DISABLE_FIND_PACKAGE_HarfBuzz")
		set(${_freetype_option} TRUE CACHE INTERNAL "")
	endforeach()
	foreach(_freetype_option "SKIP_INSTALL_HEADERS" "SKIP_INSTALL_LIBRARIES")
		set(${_freetype_option} ON CACHE INTERNAL "")
	endforeach()

#	add_subdirectory(${PROJECT_SOURCE_DIR}/Libraries/Acid/Libraries/freetype2)
	set(FREETYPE_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/Libraries/Acid/Libraries/freetype2/include")
	# Used in target_link_libraries()
	set(FREETYPE_LIBRARIES "freetype")
endif()

add_executable(Font ${FONT_SOURCES})
add_dependencies(Font Acid)

target_compile_features(Font PUBLIC cxx_std_17)
set_target_properties(Font PROPERTIES
	POSITION_INDEPENDENT_CODE ON
	FOLDER "Acid"
	)

target_include_directories(Font PRIVATE ${ACID_INCLUDE_DIR} $<$<BOOL:${FREETYPE_INCLUDE_DIRS}>:${FREETYPE_INCLUDE_DIRS}> ${FONT_INCLUDE_DIR})
target_link_libraries(Font PRIVATE Acid glfw ${FREETYPE_LIBRARIES})

if(MSVC AND "${CMAKE_BUILD_TYPE}" STREQUAL "Release")
	set_target_properties(Font PROPERTIES LINK_FLAGS "/subsystem:windows /ENTRY:mainCRTStartup")
elseif(UNIX AND APPLE)
	set_target_properties(Font PROPERTIES
		MACOSX_BUNDLE_BUNDLE_NAME "Font"
		MACOSX_BUNDLE_SHORT_VERSION_STRING ${ACID_VERSION}
		MACOSX_BUNDLE_LONG_VERSION_STRING ${ACID_VERSION}
		MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/Scripts/MacOSXBundleInfo.plist.in"
		)
endif()

add_test(NAME "Font" COMMAND "Font")

if(ACID_INSTALL_EXAMPLES)
	install(TARGETS Font
			RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
			ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
			)
endif()
