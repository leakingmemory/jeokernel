include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(scsi OBJECT scsidevice.cpp scsidevice.h scsida.cpp scsida.h)

target_include_directories(scsi PRIVATE ../../include)
target_include_directories(scsi PRIVATE ../blockdevs/include)
