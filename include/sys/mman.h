//
// Created by sigsegv on 7/16/22.
//

#ifndef JEOKERNEL_MMAN_H
#define JEOKERNEL_MMAN_H

#define PROT_NONE   0
#define PROT_READ   1
#define PROT_WRITE  2
#define PROT_EXEC   4

#define PROT_SUPPORTED  (PROT_READ | PROT_WRITE | PROT_EXEC)

#define MAP_SHARED      1
#define MAP_PRIVATE     2
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_DENYWRITE   0x800   // Nobody should be allowed to write to the file as long as the mapping
                                // This is no longer done in Linux, so it's probably safe to ignore

#define MAP_TYPE_MASK   0xF
#define MAP_SUPPORTED   (MAP_SHARED | MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS | MAP_DENYWRITE)

#endif //JEOKERNEL_MMAN_H
