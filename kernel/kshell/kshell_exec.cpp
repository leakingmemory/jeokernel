//
// Created by sigsegv on 6/8/22.
//

#include <iostream>
#include <exec/exec.h>
#include <concurrency/raw_semaphore.h>
#include <exec/process.h>
#include "kshell_exec.h"
#include <resource/referrer.h>
#include <resource/reference.h>

class kshell_exec_exec : public referrer {
private:
    std::weak_ptr<kshell_exec_exec> selfRef{};
    kshell_exec_exec() : referrer("kshell_exec_exec") {}
public:
    kshell_exec_exec(const kshell_exec_exec &) = delete;
    kshell_exec_exec(kshell_exec_exec &&) = delete;
    kshell_exec_exec &operator =(const kshell_exec_exec &) = delete;
    kshell_exec_exec &operator =(kshell_exec_exec &&) = delete;
    static std::shared_ptr<kshell_exec_exec> Create();
    std::string GetReferrerIdentifier() override;
    void Exec(kshell &shell, const std::vector<std::string> &cmd);
};

std::shared_ptr<kshell_exec_exec> kshell_exec_exec::Create() {
    std::shared_ptr<kshell_exec_exec> e{new kshell_exec_exec()};
    std::weak_ptr<kshell_exec_exec> w{e};
    e->selfRef = w;
    return e;
}

std::string kshell_exec_exec::GetReferrerIdentifier() {
    return "";
}

void kshell_exec_exec::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    std::shared_ptr<keycode_consumer> keycodeConsumer{shell.Tty()};
    if (!keycodeConsumer) {
        std::cerr << "Tty can not be subscribed for input (keyboard)\n";
        return;
    }
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    reference<kfile> dir_ref = shell.CwdRef(selfRef);
    kdirectory *dir = &(shell.Cwd());
    auto iterator = cmd.begin();
    if (iterator != cmd.end()) {
        ++iterator;
    }
    if (iterator == cmd.end()) {
        std::cerr << "No filename for exec\n";
        return;
    }
    std::string filename = *iterator;
    std::vector<std::string> env{};
    std::vector<std::string> args{};
    args.push_back(filename);
    ++iterator;
    while (iterator != cmd.end()) {
        args.push_back(*iterator);
        ++iterator;
    }
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
            litem = rootdir->CreateReference(selfRef);
        }
        if (litem) {
            kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
            if (ldir != nullptr) {
                std::cerr << "exec: is a directory: " << filename << "\n";
            } else {
                std::shared_ptr<class Exec> exec = Exec::Create(shell.Tty(), dir_ref, *dir, litem, filename, args, env, 0);
                Keyboard().consume(keycodeConsumer);
                auto process = exec->Run([] (const std::shared_ptr<Process> &process) {
                    auto pgrp = process->getpgrp();
                    auto tty = process->GetTty();
                    tty->SetPgrp(pgrp);
                });
                if (process) {
                    raw_semaphore latch{-1};
                    process->RegisterExitNotification([&latch] (intptr_t exitCode) {
                        std::cout << "Exited with code " << std::dec << exitCode << "\n";
                        latch.release();
                    });
                    process = {};
                    latch.acquire();
                }
                Keyboard().unconsume(keycodeConsumer);
            }
        } else {
            std::cerr << "exec: not found: " << filename << "\n";
        }
    } else {
        auto resolveResult = dir->Resolve(&(*rootdir), selfRef, filename);
        if (resolveResult.status != kfile_status::SUCCESS) {
            std::cerr << "Error: " << text(resolveResult.status) << "\n";
            return;
        }
        reference<kfile> litem = std::move(resolveResult.result);
        if (litem) {
            kdirectory *ldir = dynamic_cast<kdirectory *> (&(*litem));
            if (ldir != nullptr) {
                std::cerr << "exec: is a directory: " << filename << "\n";
            } else {
                std::shared_ptr<class Exec> exec = Exec::Create(shell.Tty(), dir_ref, *dir, litem, filename, args, env, 0);
                Keyboard().consume(keycodeConsumer);
                auto process = exec->Run();
                if (process) {
                    raw_semaphore latch{-1};
                    process->RegisterExitNotification([&latch] (intptr_t exitCode) {
                        std::cout << "Exited with code " << std::dec << exitCode << "\n";
                        latch.release();
                    });
                    process = {};
                    latch.acquire();
                }
                Keyboard().unconsume(keycodeConsumer);
            }
        } else {
            std::cerr << "exec: not found: " << filename << "\n";
        }
    }
}

void kshell_exec::Exec(kshell &shell, const std::vector<std::string> &cmd) {
    kshell_exec_exec::Create()->Exec(shell, cmd);
}