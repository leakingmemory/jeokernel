//
// Created by sigsegv on 11/10/23.
//

#include <files/symlink.h>
#include "kdirectory_impl_lazy.h"
#include "kdirectory_impl.h"
#include "ksymlink_impl.h"

std::shared_ptr<kdirectory_impl_lazy>
kdirectory_impl_lazy::Create(const std::shared_ptr<kdirectory_impl> &ref, const std::shared_ptr<directory_entry> &dirent,
                             const std::string &dirname) {
    std::shared_ptr<kdirectory_impl_lazy> kdir{new kdirectory_impl_lazy(dirent, dirname)};
    std::weak_ptr<kdirectory_impl_lazy> w{kdir};
    kdir->selfRef = w;
    kdir->ref = ref->CreateReference(kdir);
    return kdir;
}

std::string kdirectory_impl_lazy::GetReferrerIdentifier() {
    return "";
}

reference<kfile> kdirectory_impl_lazy::Load(std::shared_ptr<class referrer> &referrer) {
    fsreference<fileitem> fsfile{};
    {
        auto ld = dirent->LoadItem(selfRef.lock());
        if (ld.status != fileitem_status::SUCCESS || !ld.file) {
            return {};
        }
        fsfile = std::move(ld.file);
    }
    directory *fsdir = dynamic_cast<directory *> (&(*fsfile));
    if (fsdir != nullptr) {
        std::string subpath{dirname};
        if (subpath[subpath.size() - 1] != '/') {
            subpath.append("/", 1);
        }
        subpath.append(dirent->Name());
        auto refCopy = ref->CreateReference(selfRef.lock());
        auto parentDirRef = reference_dynamic_cast<kdirectory_impl>(std::move(refCopy));
        std::shared_ptr<kdirectory_impl> dir = kdirectory_impl::Create(fsfile, parentDirRef, subpath);
        auto dirRef = dir->CreateReference(referrer);
        reference<kfile> fileRef = reference_dynamic_cast<kfile>(std::move(dirRef));
        return fileRef;
    } else {
        class symlink *syml = dynamic_cast<class symlink *>(&(*fsfile));
        if (syml != nullptr) {
            auto refCopy = ref->CreateReference(selfRef.lock());
            auto parentDirRef = reference_dynamic_cast<kdirectory_impl>(std::move(refCopy));
            std::shared_ptr<ksymlink_impl> symli = ksymlink_impl::Create(fsfile, parentDirRef, dirent->Name());
            auto symliRef = symli->CreateReference(referrer);
            reference<kfile> fileRef = reference_dynamic_cast<kfile>(std::move(symliRef));
            return fileRef;
        } else {
            std::shared_ptr<kfile_impl> file = kfile_impl::Create<kfile_impl>(fsfile, dirent->Name());
            return file->CreateReference(referrer);
        }
    }
}
