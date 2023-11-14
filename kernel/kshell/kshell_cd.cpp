//
// Created by sigsegv on 6/7/22.
//

#include <iostream>
#include "kshell_cd.h"
#include <resource/referrer.h>
#include <resource/reference.h>

class kshell_cd_exec : public referrer {
private:
    std::weak_ptr<kshell_cd_exec> selfRef{};
    kshell_cd_exec() : referrer("kshell_cd_exec") {}
public:
    static std::shared_ptr<kshell_cd_exec> Create();
    kshell_cd_exec(const kshell_cd_exec &) = delete;
    kshell_cd_exec(kshell_cd_exec &&) = delete;
    kshell_cd_exec &operator =(const kshell_cd_exec &) = delete;
    kshell_cd_exec &operator =(kshell_cd_exec &&) = delete;
    std::string GetReferrerIdentifier() override;
    void Exec(kshell &shell, const std::vector<std::string> &cmd);
};

std::shared_ptr<kshell_cd_exec> kshell_cd_exec::Create() {
    std::shared_ptr<kshell_cd_exec> e{new kshell_cd_exec()};
    std::weak_ptr<kshell_cd_exec> w{e};
    e->selfRef = w;
    return e;
}

std::string kshell_cd_exec::GetReferrerIdentifier() {
    return "";
}

void kshell_cd_exec::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    auto &cwd = shell.Cwd();
    auto iterator = cmd.begin();
    if (iterator != cmd.end()) {
        ++iterator;
    }
    auto rootdir = get_kernel_rootdir();
    if (iterator == cmd.end()) {
        shell.CwdRoot();
        return;
    }
    std::string dir = *iterator;
    ++iterator;
    if (iterator != cmd.end()) {
        std::cerr << "cd: Too many arguments\n";
        return;
    }
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    kfile_result<reference<kfile>> newDirRes{};
    if (dir.starts_with("/")) {
        while (dir.starts_with("/")) {
            std::string trim{};
            trim.append(dir.c_str() + 1);
            dir = trim;
        }
        newDirRes = rootdir->Resolve(&(*rootdir), selfRef, dir);
    } else {
        newDirRes = cwd.Resolve(&(*rootdir), selfRef, dir);
    }
    if (newDirRes.status != kfile_status::SUCCESS) {
        std::cerr << "Error: " << text(newDirRes.status) << "\n";
        return;
    }
    auto newDir = std::move(newDirRes.result);
    if (!newDir) {
        std::cerr << "cd: Not found\n";
        return;
    }
    if (dynamic_cast<kdirectory *>(&(*newDir)) == nullptr) {
        std::cerr << "cd: Not a directory\n";
        return;
    }
    shell.Cwd(newDir);
}

void kshell_cd::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    kshell_cd_exec::Create()->Exec(shell, cmd);
}