//
// Created by sigsegv on 30.04.2021.
//

#ifndef JEOKERNEL_KERNELELF_H
#define JEOKERNEL_KERNELELF_H

#include <multiboot.h>
#include <klogger.h>
#include <elf.h>
#include <string>
#include <tuple>

class KernelElf {
private:
    void *ptr, *end_ptr;
    ELF kernel_elf;
#if defined(__aarch64__)
    uint64_t vsize;
#endif
public:
    KernelElf(uint64_t phys_start, uint64_t size);
#if defined(__x86_64__) || defined(__i386__)
    static KernelElf *CreateFromMultiboot(const MultibootInfoHeader &multibootInfoHeader);
#endif
    ~KernelElf();

    const ELF &elf() const;
    std::tuple<uint64_t,const char *> get_symbol(void *) const;
};

const KernelElf &get_kernel_elf();

#endif //JEOKERNEL_KERNELELF_H
