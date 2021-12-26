//
// Created by sigsegv on 12/26/21.
//

#include "framebuffer_kconsole.h"
#include "framebuffer_console.h"

framebuffer_kconsole::framebuffer_kconsole(std::shared_ptr<framebuffer_console> fbconsole) :
    fbconsole(fbconsole)
{
}

void framebuffer_kconsole::print_at(uint8_t col, uint8_t row, const char *str) {
    uint32_t pos{fbconsole->GetPosition()};
    fbconsole->SetPosition(col, row);
    while (*str) {
        fbconsole->printch(*str, 0x00FFFFFF, 0);
        ++str;
    }
    fbconsole->SetPosition(pos);
}

framebuffer_kconsole &framebuffer_kconsole::operator<<(const char *str) {
    uint32_t width{fbconsole->GetWidth()};
    uint32_t height{fbconsole->GetHeight()};
    uint32_t end{width * height};
    while (*str) {
        if (*str == '\n') {
            uint32_t pos{fbconsole->GetPosition()};
            pos -= pos % width;
            uint32_t y{pos / width};
            if ((y + 1) < height) {
                pos += width;
            } else {
                fbconsole->ShiftUpLines(1, 0);
            }
            fbconsole->SetPosition(pos);
        } else if (*str == '\r') {
            uint32_t pos{fbconsole->GetPosition()};
            pos -= pos % fbconsole->GetWidth();
            fbconsole->SetPosition(pos);
        } else {
            uint32_t pos{fbconsole->GetPosition()};
            fbconsole->printch(*str, 0x00FFFFFF, 0);
            if ((pos + 1) == end) {
                pos -= pos % width;
                fbconsole->SetPosition(pos);
                fbconsole->ShiftUpLines(1, 0);
            }
        }
        ++str;
    }
    return *this;
}
