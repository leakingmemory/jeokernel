
if(DEFINED ENV{USE_CLANG})
    set(USE_CLANG true)
endif(DEFINED ENV{USE_CLANG})

if(USE_CLANG)
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m64 -march=x86-64 -target x86_64-pc-win32-coff -fno-stack-protector -fshort-wchar -mno-red-zone -nostdinc -nostdlib -ffreestanding -DAMD64 -DEFI_LOADER ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -target x86_64-pc-win32-coff -fno-stack-protector -fshort-wchar -mno-red-zone -nostdinc -nostdlib -ffreestanding -DCLANG -DAMD64 -DEFI_LOADER ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -target x86_64-pc-win32-coff -fno-stack-protector -fshort-wchar -mno-red-zone -nostdinc -nostdlib -ffreestanding -DCLANG -DAMD64 -DEFI_LOADER ")

    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_COMPILER} -target x86_64-pc-win32-coff -fno-stack-protector -fshort-wchar -mno-red-zone -nostdinc -nostdlib -fuse-ld=lld -ffreestanding -Wl,-entry:efi_main -Wl,-subsystem:efi_application <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
else(USE_CLANG)

    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m64 -march=x86-64 -nostdinc -nostdlib -ffreestanding -fPIC -DAMD64 -DEFI_LOADER ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -march=x86-64 -nostdinc -nostdlib -ffreestanding -fPIC -DAMD64 -DEFI_LOADER ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -march=x86-64 -nostdinc -nostdlib -ffreestanding -fPIC -DAMD64 -DEFI_LOADER ")

    set(CMAKE_CXX_LINK_EXECUTABLE "ld --oformat=elf64-x86-64 -melf_x86_64 -shared -nostdlib --entry=efi_main -T ${CMAKE_SOURCE_DIR}/uefi64/uefi64.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
endif(USE_CLANG)

