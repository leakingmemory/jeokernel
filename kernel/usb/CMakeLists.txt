include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(usb OBJECT ohci.cpp ohci.h uhci.cpp uhci.h ehci.cpp ehci.h xhci.cpp xhci.h Phys32Page.cpp Phys32Page.h usb_hcd.cpp usb_hcd.h usb_port_connection.cpp usb_port_connection.h StructPool.cpp StructPool.h usb_transfer.cpp usb_transfer.h usbifacedev.cpp usbifacedev.h usbkbd.cpp usbkbd.h UsbBuffer32.cpp UsbBuffer32.h xhci_resources_32.cpp xhci_resources_32.h control_request_trait.cpp control_request_trait.h )

target_include_directories(usb PRIVATE ../../include)
