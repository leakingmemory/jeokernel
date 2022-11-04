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

tty::tty() : mtx(), sema(-1), thr([this] () {thread();}), codepage(KeyboardCodepage()), buffer(), subscribers(), stop(false) {
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
        case TCGETS:
            return ctx.Write(arg, sizeof(termios), [ctx] (void *ptr) mutable {
                termios *t = (termios *) ptr;
                t->c_iflag = 0;
                t->c_oflag = 0;
                t->c_cflag = 0;
                t->c_lflag = 0;
                t->c_line = 0;
                for (int i = 0; i < termios::ncc; i++) {
                    t->c_cc[i] = 0;
                }
                return ctx.Return(0);
            });
    }
    std::cout << "tty->ioctl(0x" << std::hex << cmd << ", 0x" << arg << std::dec << ")\n";
    return -EOPNOTSUPP;
}

bool tty::Consume(uint32_t keycode) {
    if ((keycode & KEYBOARD_CODE_BIT_RELEASE) == 0) {
        char ch[2] = {(char) codepage->Translate(keycode), 0};
        get_klogger() << ch;
        std::lock_guard lock{mtx};
        buffer.append(ch, 1);
        sema.release();
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

void InitTty() {
    ttyhandler::Create("tty");
}
