cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)

add_library(filesystems OBJECT ext2fs.cpp ext2fs.h ext2fs_inode.cpp ext2fs_inode.h ext2fs_inode_results.h ext2fs_inode_reader.cpp ext2fs_inode_reader.h ext2fs_file.cpp ext2fs_file.h ext2fs_directory.cpp ext2fs_directory.h filesystem_impls.cpp filesystems.cpp)

target_include_directories(filesystems PRIVATE ../include)

target_include_directories(filesystems PRIVATE ../../../include)
