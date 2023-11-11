//
// Created by sigsegv on 2/26/23.
//

#ifndef JEOKERNEL_PROCLAZYSTRFILE_H
#define JEOKERNEL_PROCLAZYSTRFILE_H

#include "ProcDatafile.h"
#include <string>
#include <functional>
#include <mutex>

class Process;

class ProcLazyStrfile : public ProcDatafile {
private:
    std::mutex mtx;
    std::weak_ptr<Process> process;
    std::string str{};
    bool initialized;
protected:
    ProcLazyStrfile(fsresourcelockfactory &lockfactory, const std::shared_ptr<Process> &process) : ProcDatafile(lockfactory), mtx(), process(process), str(), initialized(false) {}
public:
    std::size_t Size() override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
    virtual std::string GetContent(Process &proc) = 0;
};


#endif //JEOKERNEL_PROCLAZYSTRFILE_H
