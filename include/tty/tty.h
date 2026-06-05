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
class kshell;
class KLogger;

class tty_output {
public:
    virtual int write(const void *, size_t) = 0;
    virtual void erase(int backtrack, int erase) = 0;
    virtual ~tty_output() = default;
};

class tty_output_klogger : public tty_output {
private:
    KLogger *klogger;
public:
    tty_output_klogger(KLogger *klogger);
    int write(const void *ptr, size_t len) override;
    void erase(int backtrack, int erase) override;
    ~tty_output_klogger() override = default;
};

class tty : public keycode_consumer {
private:
    hw_spinlock mtx;
    std::weak_ptr<tty> self;
    raw_semaphore sema;
    std::unique_ptr<keyboard_source_interface> input;
    std::shared_ptr<tty_output> output;
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
    tty(const std::shared_ptr<tty_output> &output, std::unique_ptr<keyboard_source_interface> &&input);
public:
    static std::shared_ptr<tty> Create(const std::shared_ptr<tty_output> &output, std::unique_ptr<keyboard_source_interface> &&input);
    void thread();
    ~tty();
    std::shared_ptr<tty_output> GetOutput() const {
        return output;
    }
    intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg);
    bool Consume(keycode_consumer_mode mode, uint32_t keycode) override;
    bool HasInput();
    void Subscribe(std::shared_ptr<FileDescriptorHandler> handler);
    void Unsubscribe(FileDescriptorHandler *handler);
    int Read(void *ptr, int len);
    int Write(const void *ptr, size_t len);
    void Erase(int backtrack, int erase);
    void SetPgrp(pid_t pgrp);
};

#endif //JEOKERNEL_TTY_H
