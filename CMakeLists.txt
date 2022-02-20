cmake_minimum_required(VERSION 3.16)
project(fsbits)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(fileaccess)
add_subdirectory(blockdevs)

add_executable(fsbits main.cpp)

target_link_libraries(fsbits PRIVATE fileaccess)
target_link_libraries(fsbits PRIVATE blockdevs)

target_include_directories(fsbits PRIVATE include)

