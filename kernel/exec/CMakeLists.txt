include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(exec OBJECT exec.cpp process.cpp UserElf.cpp UserElf.h fdesc.cpp StdinDesc.cpp StdinDesc.h StdoutDesc.cpp StdoutDesc.h procthread.cpp usermem.cpp rseq.cpp files.cpp callctx.cpp blockthread.cpp WorkingForThread.cpp futex.cpp pipe.cpp)

target_include_directories(exec PRIVATE ../../include)
