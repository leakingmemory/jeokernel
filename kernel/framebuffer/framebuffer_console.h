//
// Created by sigsegv on 12/26/21.
//

#ifndef JEOKERNEL_FRAMEBUFFER_CONSOLE_H
#define JEOKERNEL_FRAMEBUFFER_CONSOLE_H


#include <framebuffer/drawingdisplay.h>
#include <cstdint>
#include <font/font8x8.h>

constexpr int bytesPerPixel = 4;
constexpr int cursorSize = 2 * 8;
constexpr int cursorMemoryFootprint = cursorSize * bytesPerPixel;

class framebuffer_console {
private:
    drawingdisplay &display;
    uint8_t cursorRestore[cursorMemoryFootprint];
    uint32_t cursorRestoreSize;
    uint32_t cursorX;
    uint32_t cursorY;
    uint32_t cursorWidth;
    uint32_t cursorHeight;
    uint32_t width;
    uint32_t height;
    uint32_t pos;
    uint32_t chsize;
public:
    explicit framebuffer_console(drawingdisplay &display);
    uint32_t GetWidth() {
        return width;
    }
    uint32_t GetHeight() {
        return height;
    }
    uint32_t GetPosition() {
        return pos;
    }
    void SetPosition(uint32_t pos) {
        this->pos = pos;
        if (this->pos >= chsize) {
            this->pos = this->pos % chsize;
        }
    }
    void SetPosition(int x, int y) {
        uint32_t pos = y;
        pos = pos * GetWidth();
        pos += x;
        SetPosition(pos);
    }
    void printch(uint32_t ch, uint32_t color, uint32_t background) {
        uint32_t x{(pos % width) << 3};
        uint32_t y{(pos / width) << 3};
        display.Draw8x8Char(x, y, color, background, Get8x8Character(ch));
        if (++pos > chsize) {
            pos = 0;
        }
    }
    void ShiftUpLines(int lines, uint32_t color) {
        display.ShiftUpLines(lines << 3, color);
    }
    void SetCursorVisibility(bool visibility, uint32_t color);
};


#endif //JEOKERNEL_FRAMEBUFFER_CONSOLE_H
