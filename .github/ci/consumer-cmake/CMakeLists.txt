cmake_minimum_required(VERSION 3.16)
project(cubiomes_consumer LANGUAGES C CXX)

find_package(cubiomes CONFIG REQUIRED)

add_executable(consumer_c consumer_c.c)
target_link_libraries(consumer_c PRIVATE cubiomes::cubiomes_static)

add_executable(consumer_cpp consumer_cpp.cpp)
target_link_libraries(consumer_cpp PRIVATE cubiomes::cubiomes_static)
