cmake_minimum_required(VERSION 3.16)
project(cubiomes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(CUBIOMES_BUILD_TESTS "Build regression test executable" ON)
option(CUBIOMES_STRICT "Treat warnings as errors" OFF)
option(CUBIOMES_PEDANTIC "Enable pedantic C++ warnings for library targets" OFF)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()
if (WIN32)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_WIN32")
endif()
if (UNIX)
	add_link_options(-Wl,-rpath,/usr/local/gcc-14.1.0/lib64)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

set(HEADERS
	include/finders.hpp
	include/generator.hpp
	include/layers.hpp
	include/biomenoise.hpp
	include/biomes.hpp
	include/noise.hpp
	include/rng.hpp
	include/util.hpp
	include/quadbase.hpp
	include/cpp_api.hpp
)
set(SOURCES
	src/finders.cpp
	src/generator.cpp
	src/layers.cpp
	src/biomenoise.cpp
	src/biomes.cpp
	src/noise.cpp
	src/util.cpp
	src/quadbase.cpp
)

add_library(objects OBJECT ${SOURCES})
set_property(TARGET objects PROPERTY POSITION_INDEPENDENT_CODE 1)
target_include_directories(objects
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/include
)
if (CUBIOMES_PEDANTIC)
	target_compile_options(objects PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>)
endif()
if (CUBIOMES_STRICT)
	target_compile_options(objects PRIVATE -Werror)
endif()

add_library(cubiomes SHARED $<TARGET_OBJECTS:objects>)
add_library(cubiomes_static STATIC $<TARGET_OBJECTS:objects>)
add_library(cubiomes_cpp STATIC src/cpp_api.cpp)
add_library(cubiomes::cubiomes ALIAS cubiomes)
add_library(cubiomes::cubiomes_static ALIAS cubiomes_static)
add_library(cubiomes::cubiomes_cpp ALIAS cubiomes_cpp)

target_include_directories(cubiomes
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(cubiomes_static
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(cubiomes_cpp
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(cubiomes_cpp PUBLIC cubiomes_static)

if (UNIX)
	target_link_libraries(cubiomes PUBLIC m pthread)
	target_link_libraries(cubiomes_static PUBLIC m pthread)
endif()

if (CUBIOMES_PEDANTIC)
	target_compile_options(cubiomes_cpp PRIVATE -Wpedantic)
endif()
if (CUBIOMES_STRICT)
	target_compile_options(cubiomes_cpp PRIVATE -Werror)
endif()

if (CUBIOMES_BUILD_TESTS)
	add_executable(cubiomes-tests tests/tests.cpp)
	add_executable(cubiomes-cpp-api-tests tests/tests_cpp_api.cpp)
	add_executable(cubiomes-util-tests tests/tests_util.cpp)
	target_compile_definitions(cubiomes-tests PRIVATE _POSIX_C_SOURCE=200809L)
	target_link_libraries(cubiomes-tests PRIVATE cubiomes_static)
	target_link_libraries(cubiomes-cpp-api-tests PRIVATE cubiomes_cpp)
	target_link_libraries(cubiomes-util-tests PRIVATE cubiomes_static)
	if (CUBIOMES_STRICT)
		target_compile_options(cubiomes-tests PRIVATE -Werror)
		target_compile_options(cubiomes-cpp-api-tests PRIVATE -Werror)
		target_compile_options(cubiomes-util-tests PRIVATE -Werror)
	endif()
endif()

set(CUBIOMES_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/cubiomes")

install(TARGETS cubiomes cubiomes_static cubiomes_cpp
	EXPORT cubiomesTargets
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT cubiomesTargets
	FILE cubiomesTargets.cmake
	NAMESPACE cubiomes::
	DESTINATION ${CUBIOMES_INSTALL_CMAKEDIR}
)

configure_package_config_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/cubiomesConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/cubiomesConfig.cmake
	INSTALL_DESTINATION ${CUBIOMES_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/cubiomesConfigVersion.cmake
	VERSION 1.0.0
	COMPATIBILITY SameMajorVersion
)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/cubiomesConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/cubiomesConfigVersion.cmake
	DESTINATION ${CUBIOMES_INSTALL_CMAKEDIR}
)
