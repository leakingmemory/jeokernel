include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(serial OBJECT serialport.cpp serialport.h serialtty.cpp serialtty.h)

target_include_directories(serial PRIVATE ../../include)
