//
// Created by sigsegv on 6/6/22.
//

#include <iostream>
#include "kshell_ls.h"

static void single(kfile *file, std::string name) {
    if (!file) {
        std::cout << name << " (**error)\n";
        return;
    }
    std::cout << std::oct << file->Mode() << std::dec << " " << file->Size() << " " << name;
    ksymlink *syml = dynamic_cast<ksymlink *>(file);
    if (syml != nullptr) {
        std::cout << " -> " << syml->GetLink();
    }
    std::cout << "\n";
}

class kshell_ls_list : public referrer {
private:
    std::weak_ptr<kshell_ls_list> selfRef;
    kshell_ls_list() : referrer("kshell_ls_list") {}
public:
    static std::shared_ptr<kshell_ls_list> Create();
    kshell_ls_list(const kshell_ls_list &) = delete;
    kshell_ls_list(kshell_ls_list &&) = delete;
    kshell_ls_list &operator =(const kshell_ls_list &) = delete;
    kshell_ls_list &operator =(kshell_ls_list &&) = delete;
    std::string GetReferrerIdentifier() override;
    void list(kdirectory *dir);
};

std::shared_ptr<kshell_ls_list> kshell_ls_list::Create() {
    std::shared_ptr<kshell_ls_list> ls{new kshell_ls_list()};
    std::weak_ptr<kshell_ls_list> w{ls};
    ls->selfRef = w;
    return ls;
}

std::string kshell_ls_list::GetReferrerIdentifier() {
    return "";
}

void kshell_ls_list::list(kdirectory *dir) {
    auto result = dir->Entries();
    if (result.status != kfile_status::SUCCESS) {
        std::cerr << "Error: " << text(result.status) << "\n";
    }
    std::shared_ptr<referrer> selfRef = this->selfRef.lock();
    for (auto entry : result.result) {
        auto file = entry->File(selfRef);
        single(&(*file), entry->Name());
    }
}

class kshell_ls_exec : public referrer {
private:
    std::weak_ptr<kshell_ls_exec> selfRef;
    kshell_ls_exec() : referrer("kshell_ls_exec") {}
public:
    static std::shared_ptr<kshell_ls_exec> Create();
    kshell_ls_exec(const kshell_ls_exec &) = delete;
    kshell_ls_exec(kshell_ls_exec &&) = delete;
    kshell_ls_exec &operator =(const kshell_ls_exec &) = delete;
    kshell_ls_exec &operator =(kshell_ls_exec &&) = delete;
    std::string GetReferrerIdentifier() override;
    void Exec(kdirectory *dir, const std::vector<std::string> &cmd);
};

std::shared_ptr<kshell_ls_exec> kshell_ls_exec::Create() {
    std::shared_ptr<kshell_ls_exec> ls{new kshell_ls_exec()};
    std::weak_ptr<kshell_ls_exec> w{ls};
    ls->selfRef = w;
    return ls;
}

std::string kshell_ls_exec::GetReferrerIdentifier() {
    return "";
}

void kshell_ls_exec::Exec(kdirectory *dir, const std::vector<std::string> &cmd) {
    auto iterator = cmd.begin();
    if (iterator != cmd.end()) {
        ++iterator;
    }
    if (iterator != cmd.end()) {
        std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
        do {
            std::string filename = *iterator;
            auto rootdir = get_kernel_rootdir();
            if (filename.starts_with("/")) {
                std::string resname{};
                resname.append(filename.c_str()+1);
                while (resname.starts_with("/")) {
                    std::string trim{};
                    trim.append(resname.c_str()+1);
                    resname = trim;
                }
                reference<kfile> litem;
                if (!resname.empty()) {
                    auto resolveResult = rootdir->Resolve(&(*rootdir), selfRef, resname);
                    if (resolveResult.status != kfile_status::SUCCESS) {
                        std::cerr << "Error: " << text(resolveResult.status) << "\n";
                        return;
                    }
                    litem = std::move(resolveResult.result);
                } else {
                    litem = get_kernel_rootdir()->CreateReference(selfRef);
                }
                if (litem) {
                    kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
                    if (ldir != nullptr) {
                        kshell_ls_list::Create()->list(ldir);
                    } else {
                        single(&(*litem), filename);
                    }
                } else {
                    std::cerr << "Error: " << filename << ": Not found\n";
                }
            } else {
                auto resolveResult = dir->Resolve(&(*rootdir), selfRef, filename);
                if (resolveResult.status != kfile_status::SUCCESS) {
                    std::cerr << "Error: " << text(resolveResult.status) << "\n";
                    return;
                }
                auto litem = std::move(resolveResult.result);
                if (litem) {
                    kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
                    if (ldir != nullptr) {
                        kshell_ls_list::Create()->list(ldir);
                    } else {
                        single(&(*litem), filename);
                    }
                } else {
                    std::cerr << "Error: " << filename << ": Not found\n";
                }
            }
            ++iterator;
        } while (iterator != cmd.end());
    } else {
        kshell_ls_list::Create()->list(dir);
    }
}

void kshell_ls::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    std::shared_ptr<kdirectory> dir_ref{};
    kdirectory *dir = &(shell.Cwd());
    kshell_ls_exec::Create()->Exec(dir, cmd);
}
