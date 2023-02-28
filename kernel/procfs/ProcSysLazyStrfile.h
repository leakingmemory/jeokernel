//
// Created by sigsegv on 2/28/23.
//

#ifndef JEOKERNEL_PROCSYSLAZYSTRFILE_H
#define JEOKERNEL_PROCSYSLAZYSTRFILE_H

#include "ProcDatafile.h"
#include <string>
#include <mutex>

class ProcSysLazyStrfile : public ProcDatafile {
private:
    std::mutex mtx{};
    std::string str{};
    bool initialized{false};
public:
    std::size_t Size() override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
    virtual std::string GetContent() = 0;
};


#endif //JEOKERNEL_PROCSYSLAZYSTRFILE_H
