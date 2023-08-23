//
// Created by sigsegv on 10/2/22.
//

#ifndef JEOKERNEL_TTY_H
#define JEOKERNEL_TTY_H

#include <cstdint>
#include <functional>
#include <exec/callctx.h>
#include <keyboard/keyboard.h>
#include <concurrency/raw_semaphore.h>
#include <thread>
#include <sys/types.h>

class FileDescriptorHandler;

class tty : public keycode_consumer {
private:
    hw_spinlock mtx;
    std::weak_ptr<tty> self;
    raw_semaphore sema;
    std::thread thr;
    std::shared_ptr<keyboard_codepage> codepage;
    std::string buffer;
    std::string linebuffer;
    std::vector<std::weak_ptr<FileDescriptorHandler>> subscribers;
    pid_t pgrp;
    bool hasPgrp;
    bool lineedit;
    bool signals;
    bool stop;
private:
    tty();
public:
    static std::shared_ptr<tty> Create();
    void thread();
    ~tty();
    intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg);
    bool Consume(uint32_t keycode) override;
    bool HasInput();
    void Subscribe(std::shared_ptr<FileDescriptorHandler> handler);
    void Unsubscribe(FileDescriptorHandler *handler);
    int Read(void *ptr, int len);
    void SetPgrp(pid_t pgrp);
};

#endif //JEOKERNEL_TTY_H
