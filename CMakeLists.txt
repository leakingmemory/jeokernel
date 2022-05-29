cmake_minimum_required(VERSION 3.16)
project(fsbits)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(fileaccess)
add_subdirectory(blockdevs)
add_subdirectory(hashfunc)
add_subdirectory(filesystems)

add_executable(fsbits main.cpp filepage_impl.cpp)

target_link_libraries(fsbits PRIVATE fileaccess)
target_link_libraries(fsbits PRIVATE blockdevs)
target_link_libraries(fsbits PRIVATE hashfunc)
target_link_libraries(fsbits PRIVATE filesystems)

target_include_directories(fsbits PRIVATE include)

