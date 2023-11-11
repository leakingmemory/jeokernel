//
// Created by sigsegv on 11/10/23.
//

#include "ksymlink_impl.h"

std::shared_ptr<ksymlink_impl>
ksymlink_impl::Create(const fsreference<fileitem> &file, std::shared_ptr<kfile> parent, const std::string &name) {
    return kfile_impl::Create<ksymlink_impl>(file, parent, name);
}