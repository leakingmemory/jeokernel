//
// Created by sigsegv on 8/13/23.
//

#ifndef JEOKERNEL_SYSFACCESSATIMPL_H
#define JEOKERNEL_SYSFACCESSATIMPL_H

#include <string>
#include <resource/referrer.h>

class ProcThread;

class SysFaccessatImpl : public referrer {
private:
    std::weak_ptr<SysFaccessatImpl> selfRef{};
    SysFaccessatImpl() : referrer("SysFaccesatImpl") {}
public:
    static std::shared_ptr<SysFaccessatImpl> Create();
    std::string GetReferrerIdentifier() override;
    int DoFaccessat(ProcThread &proc, int dfd, std::string filename, int mode, int flags);
};


#endif //JEOKERNEL_SYSFACCESSATIMPL_H
