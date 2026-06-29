include(../arch/x86_64/toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(scsi OBJECT scsidevice.cpp scsidevice.h scsida.cpp scsida.h CallbackLatch.cpp CallbackLatch.h)

target_include_directories(scsi PRIVATE ../../include)
target_include_directories(scsi PRIVATE ../blockdevs/include)
