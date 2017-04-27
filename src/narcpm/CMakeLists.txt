project(narcpm VERSION 0.0.0 LANGUAGES CXX)
include(../CMakeCompileFlags.txt)

set_compile_flags()

file(GLOB_RECURSE src
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(narcpm
	${src})

target_include_directories(narcpm PRIVATE src)

target_link_libraries(narcpm
	${LINKED_LIBRARIES})

install(TARGETS narcpm EXPORT narcpm
	RUNTIME DESTINATION ${CMAKE_SOURCE_DIR}/../../narcpm_instal/)

export(TARGETS narcpm FILE narcpm.cmake)
