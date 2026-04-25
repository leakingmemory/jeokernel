include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(random OBJECT prng.cpp prng.h random.cpp)

target_include_directories(random PRIVATE ../../include)
