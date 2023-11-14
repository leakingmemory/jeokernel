//
// Created by sigsegv on 11/10/23.
//

#include "ksymlink_impl.h"

std::shared_ptr<ksymlink_impl>
ksymlink_impl::Create(const fsreference<fileitem> &file, const reference<kdirectory_impl> &parent, const std::string &name) {
    auto impl = kfile_impl::Create<ksymlink_impl>(file, name);
    impl->parent = parent.CreateReference(impl);
    return impl;
}