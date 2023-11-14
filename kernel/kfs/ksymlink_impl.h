//
// Created by sigsegv on 11/10/23.
//

#ifndef JEOKERNEL_KSYMLINK_IMPL_H
#define JEOKERNEL_KSYMLINK_IMPL_H

#include "kfile_impl.h"

template <class T> class reference;

class ksymlink_impl : public kfile_impl {
    friend ksymlink;
private:
    reference<kdirectory_impl> parent;
public:
    ksymlink_impl(const std::string &name) : kfile_impl(name), parent() {}
    static std::shared_ptr<ksymlink_impl> Create(const fsreference<fileitem> &file, const reference<kdirectory_impl> &parent, const std::string &name);
};


#endif //JEOKERNEL_KSYMLINK_IMPL_H
