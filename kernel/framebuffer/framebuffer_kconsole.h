//
// Created by sigsegv on 12/26/21.
//

#ifndef JEOKERNEL_FRAMEBUFFER_KCONSOLE_H
#define JEOKERNEL_FRAMEBUFFER_KCONSOLE_H

#include <memory>
#include <klogger.h>
#include <concurrency/hw_spinlock.h>

class framebuffer_console;

class framebuffer_kconsole : public KLogger {
private:
    std::shared_ptr<framebuffer_console> fbconsole;
public:
    framebuffer_kconsole(std::shared_ptr<framebuffer_console> fbconsole);
    void print_at(uint8_t col, uint8_t row, const char *str) override;
    framebuffer_kconsole & operator << (const char *str) override;

    void MakeRoomForLinefeeds(unsigned int linefeeds);
    void SetCursorVisible(bool visibility);
};


#endif //JEOKERNEL_FRAMEBUFFER_KCONSOLE_H
