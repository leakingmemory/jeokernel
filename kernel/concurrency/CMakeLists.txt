include(../arch/x86_64/toolchain.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(concurrency OBJECT ../arch/x86_64/critical_section.cpp ../arch/x86_64/hw_spinlock.cpp ../arch/x86_64/raw_spinlock.cpp mutex.cpp raw_semaphore.cpp delay.cpp mul_semaphore.cpp pthreadwrappers.cpp)

target_include_directories(concurrency PRIVATE ../../include)
