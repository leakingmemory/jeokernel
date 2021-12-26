//
// Created by sigsegv on 12/26/21.
//

#ifndef JEOKERNEL_FRAMEBUFFER_KCONSOLE_SPINLOCKED_H
#define JEOKERNEL_FRAMEBUFFER_KCONSOLE_SPINLOCKED_H


#include <concurrency/hw_spinlock.h>
#include <memory>
#include <klogger.h>

class framebuffer_kconsole_spinlocked : public KLogger {
private:
    hw_spinlock _lock;
    std::shared_ptr<KLogger> targetObject;
public:
    framebuffer_kconsole_spinlocked(std::shared_ptr<KLogger> targetObject) : _lock(), targetObject(targetObject) {
    }
    void print_at(uint8_t col, uint8_t row, const char *str) override;
    framebuffer_kconsole_spinlocked & operator << (const char *str) override;
};


#endif //JEOKERNEL_FRAMEBUFFER_KCONSOLE_SPINLOCKED_H
