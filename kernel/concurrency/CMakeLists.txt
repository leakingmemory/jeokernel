include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(concurrency OBJECT critical_section.cpp hw_spinlock.cpp mutex.cpp raw_semaphore.cpp delay.cpp mul_semaphore.cpp raw_spinlock.cpp pthreadwrappers.cpp)

target_include_directories(concurrency PRIVATE ../../include)
