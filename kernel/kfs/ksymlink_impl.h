//
// Created by sigsegv on 11/10/23.
//

#ifndef JEOKERNEL_KSYMLINK_IMPL_H
#define JEOKERNEL_KSYMLINK_IMPL_H

#include "kfile_impl.h"

class ksymlink_impl : public kfile_impl {
    friend ksymlink;
private:
    std::shared_ptr<kfile> parent;
public:
    ksymlink_impl(std::shared_ptr<kfile> parent, const std::string &name) : kfile_impl(name), parent(parent) {}
    static std::shared_ptr<ksymlink_impl> Create(const fsreference<fileitem> &file, std::shared_ptr<kfile> parent, const std::string &name);
};


#endif //JEOKERNEL_KSYMLINK_IMPL_H
