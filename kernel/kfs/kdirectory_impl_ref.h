//
// Created by sigsegv on 11/10/23.
//

#ifndef JEOKERNEL_KDIRECTORY_IMPL_REF_H
#define JEOKERNEL_KDIRECTORY_IMPL_REF_H

#include <kfs/kfiles.h>

class kdirectory_impl_ref : public lazy_kfile {
private:
    std::shared_ptr<kfile> ref;
public:
    kdirectory_impl_ref(const std::shared_ptr<kfile> &ref) : ref(ref) {}
    std::shared_ptr<kfile> Load() override;
};


#endif //JEOKERNEL_KDIRECTORY_IMPL_REF_H
