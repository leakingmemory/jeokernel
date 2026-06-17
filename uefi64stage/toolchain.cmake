
if(DEFINED ENV{USE_CLANG})
    set(USE_CLANG true)
endif(DEFINED ENV{USE_CLANG})

if(USE_CLANG)
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m64 -march=x86-64 -target x86_64-pc-none-elf -nostdinc -nostdlib -ffreestanding -fPIE -DAMD64 -DLOADER ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -march=x86-64 -target x86_64-pc-none-elf -nostdinc -nostdlib -ffreestanding -fPIE -DCLANG -DAMD64 -DLOADER ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -march=x86-64 -target x86_64-pc-none-elf -nostdinc -nostdlib -ffreestanding -fPIE -DCLANG -DAMD64 -DLOADER ")

    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_COMPILER} -march=x86-64 -target x86_64-pc-none-elf -Wl,--entry=stage_main -fPIE -nostdinc -nostdlib -fuse-ld=lld -ffreestanding -T ${CMAKE_SOURCE_DIR}/uefi64stage/uefi64stage.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES> ")
else(USE_CLANG)

    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m64 -march=x86-64 -nostdinc -nostdlib -ffreestanding -fPIE -DAMD64 -DLOADER ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -march=x86-64 -nostdinc -nostdlib -ffreestanding -fPIE -DAMD64 -DLOADER ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -march=x86-64 -nostdinc -nostdlib -ffreestanding -fPIE -DAMD64 -DLOADER ")

    set(CMAKE_CXX_LINK_EXECUTABLE "ld --oformat=elf64-x86-64 -melf_x86_64 -fPIE -nostdlib --entry=stage_main -T ${CMAKE_SOURCE_DIR}/uefi64stage/uefi64stage.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES> ")
endif(USE_CLANG)

