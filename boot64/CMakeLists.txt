include(toolchain.cmake)

cmake_minimum_required(VERSION 3.16)
project(jeokernel)

set(CMAKE_CXX_STANDARD 20)

enable_language(C ASM)

add_subdirectory(textconsole)
add_subdirectory(kmemory)

add_library(bootstrap bootstrap.S)

add_executable(kernel m64-stage1.cpp cpuio.cpp pagetable.cpp pagealloc.cpp cpp_handlers.cpp strings.cpp klogger.cpp)

target_include_directories(kernel PRIVATE ../include)

target_link_libraries(kernel PRIVATE bootstrap)
target_link_libraries(kernel PRIVATE textconsole)
target_link_libraries(kernel PRIVATE kmemory)
