//
// Created by sigsegv on 11/10/23.
//

#ifndef JEOKERNEL_KDIRECTORY_IMPL_REF_H
#define JEOKERNEL_KDIRECTORY_IMPL_REF_H

#include <kfs/kfiles.h>
#include <resource/reference.h>
#include <resource/referrer.h>

class kdirectory_impl_ref : public lazy_kfile, public referrer {
private:
    reference<kfile_impl> ref;
private:
    kdirectory_impl_ref();
public:
    void Init(const std::shared_ptr<kdirectory_impl_ref> &selfRef, const std::shared_ptr<kdirectory_impl> &ref);
    void Init(const std::shared_ptr<kdirectory_impl_ref> &selfRef, const reference<kdirectory_impl> &ref);
    static std::shared_ptr<kdirectory_impl_ref> Create(const std::shared_ptr<kdirectory_impl> &ref);
    static std::shared_ptr<kdirectory_impl_ref> Create(const reference<kdirectory_impl> &ref);
    std::string GetReferrerIdentifier() override;
    reference<kfile> Load(std::shared_ptr<class referrer> &referrer) override;
};


#endif //JEOKERNEL_KDIRECTORY_IMPL_REF_H
