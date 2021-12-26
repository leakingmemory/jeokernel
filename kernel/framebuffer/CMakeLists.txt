include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)


add_library(framebuffer OBJECT framebuffer.cpp framebuffer_console.cpp framebuffer_console.h drawingdisplay.cpp framebuffer_kconsole.cpp framebuffer_kconsole.h framebuffer_kconsole_spinlocked.cpp framebuffer_kconsole_spinlocked.h framebuffer_kcons_with_worker_thread.cpp framebuffer_kcons_with_worker_thread.h)

target_include_directories(framebuffer PRIVATE ../../include)
