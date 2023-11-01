include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(blockdevsystem OBJECT blockdev_interface.cpp blockdevsystem.cpp
        blockdev_async_command.cpp blockdev_async_command.h filepage.cpp blockdev_node.cpp blockdev_node.h
        blockdev_devfs_node.cpp ../../include/core/blockdev_devfs_node.h blockdev_writer.cpp)

target_include_directories(blockdevsystem PRIVATE ../../include)
target_include_directories(blockdevsystem PRIVATE ../blockdevs/include)
