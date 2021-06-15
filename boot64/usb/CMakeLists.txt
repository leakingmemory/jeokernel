include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(usb OBJECT ohci.cpp ohci.h uhci.cpp uhci.h ehci.cpp ehci.h xhci.cpp xhci.h Phys32Page.cpp Phys32Page.h)

target_include_directories(usb PRIVATE ../../include)
