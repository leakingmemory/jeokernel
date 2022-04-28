include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(strutil OBJECT is_x_str.c strcat.c strcpy.c strcmp.c strlen.c strtok.c strint.cpp strchr.c memcmp.c strstr.c printf.cpp strdup.cpp strint.cpp)

target_include_directories(strutil PRIVATE ../../include)
