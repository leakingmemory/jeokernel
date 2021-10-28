include(../toolchain.cmake)

set(CMAKE_CXX_STANDARD 20)

add_library(pci OBJECT pci.cpp pci.h pci_bridge.cpp pci_bridge.h pci_irq.cpp)

target_include_directories(pci PRIVATE ../../include)
target_include_directories(pci PRIVATE ../acpi/acpica/acpica-unix-20210604/source/include)
