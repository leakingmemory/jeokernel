//
// Created by sigsegv on 11/9/23.
//

#ifndef JEOKERNEL_KDIRECTORY_IMPL_H
#define JEOKERNEL_KDIRECTORY_IMPL_H

#include "kfile_impl.h"

class kdirectory_impl : public kfile_impl {
    friend kfile_impl;
private:
    std::shared_ptr<kfile> parent;
    std::weak_ptr<kdirectory_impl> selfRef{};
    kdirectory_impl(const std::shared_ptr<kfile> &parent, const std::string &kpath) : kfile_impl(kpath), parent(parent) {}
public:
    void Init(std::shared_ptr<kdirectory_impl> &selfRef);
    static std::shared_ptr<kdirectory_impl> Create(const fsreference<fileitem> &file, const std::shared_ptr<kfile> &parent, const std::string &kpath);
    kfile_result<std::vector<std::shared_ptr<kdirent>>> Entries(std::shared_ptr<kfile> this_impl);
    std::shared_ptr<filesystem> Unmount();
    void Mount(const std::string &devname, const std::string &fstype, const std::string &mntopts, const std::shared_ptr<filesystem> &fs);
    std::string Kpath();
};

#endif //JEOKERNEL_KDIRECTORY_IMPL_H
