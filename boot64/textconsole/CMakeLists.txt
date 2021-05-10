include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)


add_library(textconsole OBJECT b8000.cpp b8000.h b8000logger.cpp b8000logger.h)

target_include_directories(textconsole PRIVATE ../../include)
