
if(DEFINED ENV{USE_CLANG})
    set(USE_CLANG true)
endif(DEFINED ENV{USE_CLANG})

if(USE_CLANG)
    #set(CMAKE_C_COMPILER /usr/bin/clang-11)
    #set(CMAKE_CXX_COMPILER /usr/bin/clang++-11)
    #set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -target x86_64-pc-none-elf ")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m64 -march=x86-64 -nostdinc -fPIC -mno-red-zone -nostdlib -ffreestanding -fno-stack-protector ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -target x86_64-pc-none-elf -g -nostdinc -fPIC -mno-red-zone -nostdlib -ffreestanding -fno-stack-protector -fno-sanitize=all -DCLANG")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -target x86_64-pc-none-elf -g -nostdinc -fPIC -mno-red-zone -nostdlib -ffreestanding -fno-stack-protector -fno-sanitize=all -DCLANG")

    IF(${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
        set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_LINKER> -nostdlib --pic-executable -T ${CMAKE_SOURCE_DIR}/kernel.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
    ELSE()
        set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_COMPILER} -target x86_64-pc-none-elf¨ -mno-red-zone -nostdinc -fPIC -nostdlib -fuse-ld=lld -ffreestanding -fno-stack-protector -T ${CMAKE_SOURCE_DIR}/kernel.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES> -lgcc")
    ENDIF()
else(USE_CLANG)

    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m64 -march=x86-64 -nostdinc -fPIC -mno-red-zone -nostdlib -ffreestanding")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -march=x86-64 -g -nostdinc -fPIC -mno-red-zone -nostdlib -ffreestanding")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -march=x86-64 -g -nostdinc -fPIC -mno-red-zone -nostdlib -ffreestanding")

    set(CMAKE_CXX_LINK_EXECUTABLE "g++ -Wl,--oformat=elf64-x86-64 -nostdlib -fPIC -mno-red-zone -fstack-protector -ffreestanding -T ${CMAKE_SOURCE_DIR}/kernel.ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
endif(USE_CLANG)

