include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(kfs OBJECT kfiles.cpp kdirectory_impl.cpp kdirectory_impl.h kfile_impl.cpp kfile_impl.h kdirectory_impl_ref.cpp kdirectory_impl_ref.h ksymlink_impl.cpp ksymlink_impl.h kdirectory_impl_lazy.cpp kdirectory_impl_lazy.h)

target_include_directories(kfs PRIVATE ../../include)
target_include_directories(kfs PRIVATE ../blockdevs/include)
