//
// Created by sigsegv on 6/7/22.
//

#include <iostream>
#include "kshell_cat.h"
#include <resource/referrer.h>
#include <resource/reference.h>

class kshell_cat_exec : public referrer {
private:
    std::weak_ptr<kshell_cat_exec> selfRef{};
    uint8_t buf[256];
    kshell_cat_exec() : referrer("kshell_cat_exec") {}
public:
    kshell_cat_exec(const kshell_cat_exec &) = delete;
    kshell_cat_exec(kshell_cat_exec &&) = delete;
    kshell_cat_exec &operator =(const kshell_cat_exec &) = delete;
    kshell_cat_exec &operator =(kshell_cat_exec &&) = delete;
    static std::shared_ptr<kshell_cat_exec> Create();
    std::string GetReferrerIdentifier() override;
    void Exec(kshell &shell, const std::vector<std::string> &cmd);
};

std::shared_ptr<kshell_cat_exec> kshell_cat_exec::Create() {
    std::shared_ptr<kshell_cat_exec> e{new kshell_cat_exec()};
    std::weak_ptr<kshell_cat_exec> w{e};
    e->selfRef = w;
    return e;
}

std::string kshell_cat_exec::GetReferrerIdentifier() {
    return "";
}

void kshell_cat_exec::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    std::shared_ptr<kdirectory> dir_ref{};
    kdirectory *dir = &(shell.Cwd());
    auto iterator = cmd.begin();
    if (iterator != cmd.end()) {
        ++iterator;
    }
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto rootdir = get_kernel_rootdir();
    std::string output{};
    while (iterator != cmd.end()) {
        std::string filename = *iterator;
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
                litem = rootdir->CreateReference(selfRef);
            }
            kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
            if (ldir != nullptr) {
                std::cerr << "cat: is a directory: " << filename << "\n";
            } else {
                uint64_t offset{0};
                while (true) {
                    auto readResult = litem->Read(offset, &(buf[0]), sizeof(buf));
                    auto rd = readResult.result;
                    if (rd == 0 && readResult.status != kfile_status::SUCCESS) {
                        std::cerr << "Error: " << text(readResult.status) << "\n";
                        return;
                    }
                    if (rd == 0) {
                        break;
                    }
                    offset += rd;
                    output.clear();
                    output.append((char *) &(buf[0]), rd);
                    std::cout << output;
                }
            }
        } else {
            auto resolveResult = dir->Resolve(&(*rootdir), selfRef, filename);
            if (resolveResult.status != kfile_status::SUCCESS) {
                std::cerr << "Error: " << text(resolveResult.status) << "\n";
                return;
            }
            reference<kfile> litem = std::move(resolveResult.result);
            kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
            if (ldir != nullptr) {
                std::cerr << "cat: is a directory: " << filename << "\n";
            } else {
                uint64_t offset{0};
                while (true) {
                    auto readResult = litem->Read(offset, &(buf[0]), sizeof(buf));
                    auto rd = readResult.result;
                    if (rd == 0 && readResult.status != kfile_status::SUCCESS) {
                        std::cerr << "Error: " << text(readResult.status) << "\n";
                        return;
                    }
                    if (rd == 0) {
                        break;
                    }
                    offset += rd;
                    output.clear();
                    output.append((char *) &(buf[0]), rd);
                    std::cout << output;
                }
            }
        }
        ++iterator;
    }
}

void kshell_cat::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    kshell_cat_exec::Create()->Exec(shell, cmd);
}