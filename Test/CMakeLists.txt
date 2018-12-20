file(GLOB_RECURSE TEST_HEADER_FILES
		"*.h"
		"*.hpp"
		)
file(GLOB_RECURSE TEST_SOURCE_FILES
		"*.c"
		"*.cpp"
		"*.rc"
		)
add_executable(Test ${TEST_HEADER_FILES} ${TEST_SOURCE_FILES})
add_dependencies(Test Sources)
set(SOURCES_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/Sources/")

target_compile_features(Test PUBLIC cxx_std_17)
target_link_libraries(Test Sources)
target_include_directories(Test PRIVATE ${SOURCES_INCLUDE_DIR} ${TEST_INCLUDE_DIR})
