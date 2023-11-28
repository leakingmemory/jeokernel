//
// Created by sigsegv on 11/9/23.
//

#ifndef JEOKERNEL_KDIRECTORY_IMPL_H
#define JEOKERNEL_KDIRECTORY_IMPL_H

#include "kfile_impl.h"

class kdirectory_impl : public kfile_impl {
    friend kfile_impl;
private:
    reference<kfile> parent;
    std::weak_ptr<kdirectory_impl> selfRef{};
    kdirectory_impl(const std::string &kpath);
public:
    void Init(std::shared_ptr<kdirectory_impl> &selfRef, const std::shared_ptr<kdirectory_impl> &parent);
    void Init(std::shared_ptr<kdirectory_impl> &selfRef, const reference<kdirectory_impl> &parent);
    static std::shared_ptr<kdirectory_impl> Create(const fsreference<fileitem> &file, const std::shared_ptr<kdirectory_impl> &parent, const std::string &kpath);
    static std::shared_ptr<kdirectory_impl> Create(const fsreference<fileitem> &file, const reference<kdirectory_impl> &parent, const std::string &kpath);
    static std::shared_ptr<kdirectory_impl> CreateRoot();
    kfile_result<std::vector<std::shared_ptr<kdirent>>> Entries();
    std::shared_ptr<filesystem> Unmount();
    void Mount(const std::string &devname, const std::string &fstype, const std::string &mntopts, const std::shared_ptr<filesystem> &fs);
    std::string Kpath();
    kfile_result<std::shared_ptr<kdirectory_impl>> CreateDirectory(std::string filename, uint16_t mode);
};

#endif //JEOKERNEL_KDIRECTORY_IMPL_H
