//
// Created by sigsegv on 11/10/23.
//

#ifndef JEOKERNEL_KDIRECTORY_IMPL_LAZY_H
#define JEOKERNEL_KDIRECTORY_IMPL_LAZY_H

#include <kfs/kfiles.h>
#include <files/fsreferrer.h>
#include <files/directory.h>

class kdirectory_impl_lazy : public lazy_kfile, public referrer, public fsreferrer {
private:
    std::weak_ptr<kdirectory_impl_lazy> selfRef{};
    reference<kfile_impl> ref;
    std::shared_ptr<directory_entry> dirent;
    std::string dirname;
private:
    kdirectory_impl_lazy(const std::shared_ptr<directory_entry> &dirent, const std::string &dirname) : referrer("kdirectory_impl_lazy"), fsreferrer("kdirectory_impl_lazy"), ref(), dirent(dirent), dirname(dirname) {}
public:
    static std::shared_ptr<kdirectory_impl_lazy> Create(const std::shared_ptr<kdirectory_impl> &ref, const std::shared_ptr<directory_entry> &dirent, const std::string &dirname);
    std::string GetReferrerIdentifier() override;
    reference<kfile> Load(std::shared_ptr<class referrer> &referrer) override;
};


#endif //JEOKERNEL_KDIRECTORY_IMPL_LAZY_H
