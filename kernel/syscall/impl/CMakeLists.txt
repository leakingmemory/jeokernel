include(../../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(syscall_impl OBJECT SysOpenImpl.cpp SysOpenImpl.h SysCloneImpl.cpp SysCloneImpl.h
        SysFaccessatImpl.cpp SysFaccessatImpl.h SysMembarrierImpl.cpp SysMembarrierImpl.h)

target_include_directories(syscall_impl PRIVATE ../../../include)
