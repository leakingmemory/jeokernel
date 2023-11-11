//
// Created by sigsegv on 11/10/23.
//

#include <files/symlink.h>
#include "kdirectory_impl_lazy.h"
#include "kdirectory_impl.h"
#include "ksymlink_impl.h"

std::shared_ptr<kdirectory_impl_lazy>
kdirectory_impl_lazy::Create(const std::shared_ptr<kfile> &ref, const std::shared_ptr<directory_entry> &dirent,
                             const std::string &dirname) {
    std::shared_ptr<kdirectory_impl_lazy> kdir{new kdirectory_impl_lazy(ref, dirent, dirname)};
    std::weak_ptr<kdirectory_impl_lazy> w{kdir};
    kdir->selfRef = w;
    return kdir;
}

std::string kdirectory_impl_lazy::GetReferrerIdentifier() {
    return "";
}

std::shared_ptr<kfile> kdirectory_impl_lazy::Load() {
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
        std::shared_ptr<kdirectory_impl> dir = kdirectory_impl::Create(fsfile, ref, subpath);
        return dir;
    } else {
        class symlink *syml = dynamic_cast<class symlink *>(&(*fsfile));
        if (syml != nullptr) {
            std::shared_ptr<ksymlink_impl> symli = ksymlink_impl::Create(fsfile, ref, dirent->Name());
            return symli;
        } else {
            std::shared_ptr<kfile_impl> file = kfile_impl::Create<kfile_impl>(fsfile, dirent->Name());
            return file;
        }
    }
}
