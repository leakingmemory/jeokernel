
#set(USE_CLANG true)

if(USE_CLANG)
    set(CMAKE_C_COMPILER /usr/bin/clang-11)
    set(CMAKE_CXX_COMPILER /usr/bin/clang++-11)
    #set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -target i686-pc-none-elf ")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m32 -march=i686 -nostdinc -nostdlib -ffreestanding -DIA32 -DLOADER ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -target i686-pc-none-elf -nostdinc -nostdlib -ffreestanding -DCLANG -DIA32 -DLOADER ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -target i686-pc-none-elf -nostdinc -nostdlib -ffreestanding -DCLANG -DIA32 -DLOADER ")

    set(CMAKE_CXX_LINK_EXECUTABLE "clang++-11 -target i686-pc-none-elf -Wl,-no-pie -nostdinc -nostdlib -fuse-ld=lld -ffreestanding -T ${CMAKE_SOURCE_DIR}/boot32.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES> ")
else(USE_CLANG)

    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m32 -march=i686 -nostdinc -nostdlib -ffreestanding -DIA32 -DLOADER ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -march=i686 -nostdinc -nostdlib -ffreestanding -DIA32 -DLOADER ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -march=i686 -nostdinc -nostdlib -ffreestanding -DIA32 -DLOADER ")

    set(CMAKE_CXX_LINK_EXECUTABLE "ld --oformat=elf32-i386 -melf_i386 -no-pie -nostdlib -T ${CMAKE_SOURCE_DIR}/boot32.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES> ")
endif(USE_CLANG)

