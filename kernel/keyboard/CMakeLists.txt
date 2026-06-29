include(../arch/x86_64/toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(keyboard OBJECT keyboard.cpp keyboard_en.cpp)

target_include_directories(keyboard PRIVATE ../../include)
