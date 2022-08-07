include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(sha2 OBJECT sha2alg.cpp ../../include/hash/sha2alg.h)

target_include_directories(sha2 PRIVATE ../../include)
