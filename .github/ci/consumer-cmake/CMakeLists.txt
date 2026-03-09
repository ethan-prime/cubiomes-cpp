cmake_minimum_required(VERSION 3.16)
project(cubiomes_consumer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(cubiomes CONFIG REQUIRED)

add_executable(consumer_c consumer_c.c)
target_link_libraries(consumer_c PRIVATE cubiomes::cubiomes_static)

add_executable(consumer_cpp consumer_cpp.cpp)
target_link_libraries(consumer_cpp PRIVATE cubiomes::cubiomes_static)

add_executable(consumer_cpp_api consumer_cpp_api.cpp)
target_link_libraries(consumer_cpp_api PRIVATE cubiomes::cubiomes_cpp)
