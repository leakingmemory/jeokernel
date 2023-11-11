//
// Created by sigsegv on 11/10/23.
//

#ifndef JEOKERNEL_KDIRECTORY_IMPL_LAZY_H
#define JEOKERNEL_KDIRECTORY_IMPL_LAZY_H

#include <kfs/kfiles.h>
#include <files/fsreferrer.h>
#include <files/directory.h>

class kdirectory_impl_lazy : public lazy_kfile, public fsreferrer {
private:
    std::weak_ptr<kdirectory_impl_lazy> selfRef{};
    std::shared_ptr<kfile> ref;
    std::shared_ptr<directory_entry> dirent;
    std::string dirname;
private:
    kdirectory_impl_lazy(const std::shared_ptr<kfile> &ref, const std::shared_ptr<directory_entry> &dirent, const std::string &dirname) : fsreferrer("kdirectory_impl_lazy"), ref(ref), dirent(dirent), dirname(dirname) {}
public:
    static std::shared_ptr<kdirectory_impl_lazy> Create(const std::shared_ptr<kfile> &ref, const std::shared_ptr<directory_entry> &dirent, const std::string &dirname);
    std::string GetReferrerIdentifier() override;
    std::shared_ptr<kfile> Load() override;
};


#endif //JEOKERNEL_KDIRECTORY_IMPL_LAZY_H
