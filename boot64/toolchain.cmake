
if(DEFINED ENV{USE_CLANG})
    set(USE_CLANG true)
endif(DEFINED ENV{USE_CLANG})

if(USE_CLANG)
    #set(CMAKE_C_COMPILER /usr/bin/clang-11)
    #set(CMAKE_CXX_COMPILER /usr/bin/clang++-11)
    #set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -target x86_64-pc-none-elf ")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m64 -march=x86-64 -nostdinc -mno-red-zone -nostdlib -ffreestanding ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -target x86_64-pc-none-elf -g -nostdinc -mno-red-zone -nostdlib -ffreestanding -fno-sanitize=all -DCLANG")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -target x86_64-pc-none-elf -g -nostdinc -mno-red-zone -nostdlib -ffreestanding -fno-sanitize=all -DCLANG")

    set(CMAKE_CXX_LINK_EXECUTABLE "clang++-11 -target x86_64-pc-none-elf -Wl,-no-pie -mno-red-zone -nostdinc -nostdlib -fuse-ld=lld -ffreestanding -T ${CMAKE_SOURCE_DIR}/kernel.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES> -lgcc -lgcc_eh -lsupc++")
else(USE_CLANG)

    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m64 -march=x86-64 -nostdinc -mno-red-zone -nostdlib -ffreestanding")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -march=x86-64 -g -nostdinc -mno-red-zone -nostdlib -ffreestanding")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -march=x86-64 -g -nostdinc -mno-red-zone -nostdlib -ffreestanding")

    set(CMAKE_CXX_LINK_EXECUTABLE "g++ -Wl,--oformat=elf64-x86-64 -no-pie -nostdlib -mno-red-zone -fstack-protector -ffreestanding -T ${CMAKE_SOURCE_DIR}/kernel.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES> -lgcc_eh -lsupc++")
endif(USE_CLANG)

