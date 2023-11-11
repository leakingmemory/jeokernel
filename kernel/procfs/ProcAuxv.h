//
// Created by sigsegv on 2/23/23.
//

#ifndef JEOKERNEL_PROCAUXV_H
#define JEOKERNEL_PROCAUXV_H

#include "ProcDatafile.h"
#include "elf.h"
#include <vector>

class ProcAuxv : public ProcDatafile {
private:
    std::shared_ptr<const std::vector<ELF64_auxv>> auxv;
    ProcAuxv(fsresourcelockfactory &lockfactory, const std::shared_ptr<const std::vector<ELF64_auxv>> &auxv) : ProcDatafile(lockfactory), auxv(auxv) {}
public:
    static std::shared_ptr<ProcAuxv> Create(const std::shared_ptr<const std::vector<ELF64_auxv>> &auxv);
    std::size_t Size() override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
};


#endif //JEOKERNEL_PROCAUXV_H
