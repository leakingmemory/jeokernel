if (ARCH STREQUAL "aarch64")
    include(../arch/aarch64/toolchain.cmake)
else()
    include(../arch/x86_64/toolchain.cmake)
endif()

set(CMAKE_CXX_STANDARD 23)

if (ARCH STREQUAL "aarch64")
    add_library(concurrency OBJECT ../arch/aarch64/critical_section.cpp ../arch/aarch64/hw_spinlock.cpp ../arch/aarch64/raw_spinlock.cpp)
else()
    add_library(concurrency OBJECT ../arch/x86_64/critical_section.cpp ../arch/x86_64/hw_spinlock.cpp ../arch/x86_64/raw_spinlock.cpp mutex.cpp raw_semaphore.cpp delay.cpp mul_semaphore.cpp pthreadwrappers.cpp)
endif()

target_include_directories(concurrency PRIVATE ../../include)
