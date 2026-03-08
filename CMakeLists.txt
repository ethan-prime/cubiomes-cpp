cmake_minimum_required(VERSION 3.16)
project(cubiomes LANGUAGES C CXX)

option(CUBIOMES_BUILD_TESTS "Build regression test executable" ON)
option(CUBIOMES_STRICT "Treat warnings as errors" OFF)
option(CUBIOMES_PEDANTIC "Enable pedantic C++ warnings for library targets" OFF)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()
if (WIN32)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_WIN32")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_WIN32")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

set(HEADERS
	finders.h
	generator.h
	layers.h
	biomenoise.h
	biomes.h
	noise.h
	rng.h
	util.h
	quadbase.h
)
set(SOURCES
	finders.cpp
	generator.cpp
	layers.cpp
	biomenoise.cpp
	biomes.cpp
	noise.cpp
	util.cpp
	quadbase.cpp
)

add_library(objects OBJECT ${SOURCES})
set_property(TARGET objects PROPERTY POSITION_INDEPENDENT_CODE 1)
if (CUBIOMES_PEDANTIC)
	target_compile_options(objects PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>)
endif()
if (CUBIOMES_STRICT)
	target_compile_options(objects PRIVATE -Werror)
endif()

add_library(cubiomes SHARED $<TARGET_OBJECTS:objects>)
add_library(cubiomes_static STATIC $<TARGET_OBJECTS:objects>)

if (CUBIOMES_BUILD_TESTS)
	add_executable(cubiomes-tests tests.cpp)
	target_compile_definitions(cubiomes-tests PRIVATE _POSIX_C_SOURCE=200809L)
	target_link_libraries(cubiomes-tests PRIVATE cubiomes_static)
	if (CUBIOMES_STRICT)
		target_compile_options(cubiomes-tests PRIVATE -Werror)
	endif()
	if (UNIX)
		target_link_libraries(cubiomes-tests PRIVATE m pthread)
	endif()
endif()

install(TARGETS cubiomes cubiomes_static DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include)
