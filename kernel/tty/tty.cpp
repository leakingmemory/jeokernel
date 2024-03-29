//
// Created by sigsegv on 9/22/22.
//

#include "ttyhandler.h"
#include <tty/tty.h>
#include <tty/ttyinit.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <iostream>
#include <termios.h>
#include <exec/fdesc.h>
#include <exec/process.h>
#include <exec/procthread.h>
#include <kshell/kshell.h>
#include <kshell/kshell_commands.h>
#include <prockshell/prockshell.h>

tty::tty() : mtx(), self(), sema(-1), thr([this] () {thread();}), codepage(KeyboardCodepage()), buffer(), linebuffer(), subscribers(), pgrp(0), hasPgrp(false), lineedit(true), signals(false), stop(false) {
}

std::shared_ptr<tty> tty::Create() {
    std::shared_ptr<tty> inst{new tty()};
    std::weak_ptr<tty> weak{inst};
    inst->self = weak;
    return inst;
}

void tty::thread() {
    std::this_thread::set_name("[tty]");
    std::vector<std::weak_ptr<FileDescriptorHandler>> queue;
    while (true) {
        sema.acquire();
        {
            std::lock_guard lock{mtx};
            if (stop) {
                return;
            }
            for (const auto &q : subscribers) {
                queue.push_back(q);
            }
        }
        for (auto &weak : queue) {
            std::shared_ptr<FileDescriptorHandler> q = weak.lock();
            if (!q) {
                continue;
            }
            q->Notify();
        }
        queue.clear();
    }
}

tty::~tty() {
    {
        std::lock_guard lock{mtx};
        stop = true;
        sema.release();
    }
    thr.join();
}

intptr_t tty::ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) {
    switch (cmd) {
        case TCGETS: {
            std::shared_ptr<tty> ref = self.lock();
            return ctx.Write(arg, sizeof(termios), [ref, ctx](void *ptr) mutable {
                termios *t = (termios *) ptr;
                {
                    std::lock_guard lock{ref->mtx};
                    t->c_iflag = 0;
                    t->c_oflag = 0;
                    t->c_cflag = 0;
                    t->c_lflag = ref->signals ? ISIG : 0;
                    t->c_line = 0;
                    for (int i = 0; i < termios::ncc; i++) {
                        t->c_cc[i] = 0;
                    }
                }
                return ctx.Return(0);
            });
        }
        case TCSETSW: {
            std::shared_ptr<tty> ref = self.lock();
            return ctx.Read(arg, sizeof(termios), [ref, ctx](const void *ptr) mutable {
                const termios &t = *((const termios *) ptr);
                if (t.c_iflag != 0) {
                    std::cout << "drain & set termios(" << std::hex << t.c_iflag << ", " << t.c_oflag << ", "
                              << t.c_cflag
                              << ", " << t.c_lflag << ", " << t.c_line << std::dec << ")\n";
                    return ctx.Return(-EOPNOTSUPP);
                }
                if (t.c_oflag != 0) {
                    std::cout << "drain & set termios(" << std::hex << t.c_iflag << ", " << t.c_oflag << ", "
                              << t.c_cflag
                              << ", " << t.c_lflag << ", " << t.c_line << std::dec << ")\n";
                    return ctx.Return(-EOPNOTSUPP);
                }
                if (t.c_cflag != 0) {
                    std::cout << "drain & set termios(" << std::hex << t.c_iflag << ", " << t.c_oflag << ", "
                              << t.c_cflag
                              << ", " << t.c_lflag << ", " << t.c_line << std::dec << ")\n";
                    return ctx.Return(-EOPNOTSUPP);
                }
                if (t.c_lflag != 0 && t.c_lflag != ISIG /* Signals on Ctr+C, etc */) {
                    std::cout << "drain & set termios(" << std::hex << t.c_iflag << ", " << t.c_oflag << ", "
                              << t.c_cflag
                              << ", " << t.c_lflag << ", " << t.c_line << std::dec << ")\n";
                    return ctx.Return(-EOPNOTSUPP);
                }
                std::lock_guard lock{ref->mtx};
                ref->signals = (t.c_cflag & ISIG) != 0;
                return ctx.Return(0);
            });
        }
        case TIOCGPGRP: {
            std::shared_ptr<tty> ref = self.lock();
            return ctx.Write(arg, sizeof(pid_t), [ref, ctx](const void *ptr) mutable {
                pid_t pgrp;
                bool hasPgrp;
                {
                    std::lock_guard lock{ref->mtx};
                    pgrp = ref->pgrp;
                    hasPgrp = ref->hasPgrp;
                }
                if (!hasPgrp) {
                    pgrp = Process::GetMaxPid() + 1;
                    if (pgrp < Process::GetMaxPid()) {
                        pgrp = Process::GetMaxPid();
                    }
                }
                *((pid_t *) ptr) = pgrp;
                return resolve_return_value::Return(0);
            });
        }
        case TIOCSPGRP: {
            std::shared_ptr<tty> ref = self.lock();
            return ctx.Read(arg, sizeof(pid_t), [ref, ctx](const void *ptr) mutable {
                pid_t pgrp = *((pid_t *) ptr);
                std::cout << "TIOCSPGRP(" << pgrp << ")\n";
                ref->SetPgrp(pgrp);
                return resolve_return_value::Return(0);
            });
        }
        case TIOCGWINSZ:
            return ctx.Write(arg, sizeof(winsize), [ctx] (void *ptr) mutable {
                auto &kl = get_klogger();
                uint32_t width, height;
                kl.GetDimensions(width, height);
                winsize wz{};
                wz.ws_col = width;
                wz.ws_row = height;
                wz.ws_xpixel = 0;
                wz.ws_ypixel = 0;
                memcpy(ptr, &wz, sizeof(wz));
                return ctx.Return(0);
            });
        case TIOCSWINSZ:
            return ctx.Read(arg, sizeof(winsize), [ctx] (const void *ptr) mutable {
                const winsize &wz = *((const winsize *) ptr);
                auto &kl = get_klogger();
                uint32_t width, height;
                kl.GetDimensions(width, height);
                if (width == wz.ws_col && height == wz.ws_row) {
                    return ctx.Return(0);
                }
                std::cout << "error: tty setwinsize (" << wz.ws_col << ", " << wz.ws_row << ", " << wz.ws_xpixel << ", " << wz.ws_ypixel << ")\n";
                return ctx.Return(-EOPNOTSUPP);
            });
    }
    std::cout << "tty->ioctl(0x" << std::hex << cmd << ", 0x" << arg << std::dec << ")\n";
    return -EOPNOTSUPP;
}

bool tty::Consume(uint32_t keycode) {
    if ((keycode & KEYBOARD_CODE_BIT_RELEASE) == 0) {
        char ch[2] = {(char) codepage->Translate(keycode), 0};
        if ((ch[0] == 'c' || ch[0] == 'C') && (keycode & (KEYBOARD_CODE_BIT_LCONTROL | KEYBOARD_CODE_BIT_RCONTROL)) != 0) {
            get_klogger() << "^C\n";
            if (pgrp > 0) {
                auto *scheduler = get_scheduler();
                auto processes = std::make_shared<std::vector<std::shared_ptr<Process>>>();
                auto pids = std::make_shared<std::vector<pid_t>>();
                scheduler->all_tasks([pids, processes] (task &t) {
                    auto *procthread = t.get_resource<ProcThread>();
                    if (procthread == nullptr) {
                        return;
                    }
                    auto process = procthread->GetProcess();
                    auto pid = process->getpid();
                    bool found{false};
                    for (const auto fpid : *pids) {
                        if (pid == fpid) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        pids->push_back(pid);
                        processes->push_back(process);
                    }
                });
                for (const auto &process : *processes) {
                    if (process->getpgrp() == pgrp) {
                        if (process->setsignal(SIGINT) > 0) {
                            process->CallAbort();
                        }
                    }
                }
            }
            return true;
        } else if ((ch[0] == 's' || ch[0] == 'S') && (keycode & (KEYBOARD_CODE_BIT_LCONTROL | KEYBOARD_CODE_BIT_RCONTROL)) != 0 && (keycode & KEYBOARD_CODE_BIT_LALT) != 0) {
            std::shared_ptr<tty> ref = self.lock();
            std::shared_ptr<kshell> shell = kshell::Create(ref);
            shell->OnExit([ref] () {
                Keyboard().consume(ref);
            });
            kshell_commands cmds{*shell};
            prockshell proc{*shell};
            return false; // Unconsume keycodes, need reconnect after shell
        }
        std::lock_guard lock{mtx};
        if (lineedit) {
            if (ch[0] == '\n') {
                get_klogger() << ch;
                linebuffer.append(ch, 1);
                buffer.append(linebuffer);
                linebuffer.resize(0);
                sema.release();
            } else if (ch[0] == '\10') {
                auto s = linebuffer.size();
                if (s > 0) {
                    get_klogger().erase(1, 1);
                    linebuffer.resize(s - 1);
                }
            } else {
                get_klogger() << ch;
                linebuffer.append(ch, 1);
            }
        } else {
            get_klogger() << ch;
            buffer.append(ch, 1);
            sema.release();
        }
    }
    return true;
}

bool tty::HasInput() {
    std::lock_guard lock{mtx};
    return !buffer.empty();
}

void tty::Subscribe(std::shared_ptr<FileDescriptorHandler> handler) {
    std::lock_guard lock{mtx};
    std::weak_ptr<FileDescriptorHandler> weak{handler};
    subscribers.push_back(weak);
}

void tty::Unsubscribe(FileDescriptorHandler *handler) {
    std::lock_guard lock{mtx};
    auto iterator = subscribers.begin();
    while (iterator != subscribers.end()) {
        auto weak = *iterator;
        std::shared_ptr<FileDescriptorHandler> shared = weak.lock();
        if (!shared) {
            subscribers.erase(iterator);
            continue;
        }
        if (shared == handler) {
            subscribers.erase(iterator);
            return;
        }
        ++iterator;
    }
}

int tty::Read(void *ptr, int len) {
    if (len <= 0) {
        return -EINVAL;
    }
    std::lock_guard lock{mtx};
    auto sz = buffer.size();
    if (sz == 0) {
        return -EAGAIN;
    }
    if (sz <= len) {
        memcpy(ptr, buffer.data(), sz);
        buffer.erase(0, sz);
        return sz;
    }
    memcpy(ptr, buffer.data(), len);
    buffer.erase(0, len);
    return len;
}

void tty::SetPgrp(pid_t pgrp) {
    std::lock_guard lock{mtx};
    this->pgrp = pgrp;
    this->hasPgrp = true;
}

void InitTty() {
    ttyhandler::Create("tty");
}
