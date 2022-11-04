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

class FileDescriptorHandler;

class tty : public keycode_consumer {
private:
    hw_spinlock mtx;
    raw_semaphore sema;
    std::thread thr;
    std::shared_ptr<keyboard_codepage> codepage;
    std::string buffer;
    std::vector<std::weak_ptr<FileDescriptorHandler>> subscribers;
    bool stop;
public:
    tty();
    void thread();
    ~tty();
    intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg);
    bool Consume(uint32_t keycode) override;
    bool HasInput();
    void Subscribe(std::shared_ptr<FileDescriptorHandler> handler);
    void Unsubscribe(FileDescriptorHandler *handler);
    int Read(void *ptr, int len);
};

#endif //JEOKERNEL_TTY_H
