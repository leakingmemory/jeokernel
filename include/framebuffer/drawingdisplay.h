//
// Created by sigsegv on 12/25/21.
//

#ifndef JEOKERNEL_DRAWINGDISPLAY_H
#define JEOKERNEL_DRAWINGDISPLAY_H

#include <cstdint>

struct framebuffer_line {
    uint8_t *start;
    uint32_t size;

    uint8_t *begin() {
        return start;
    }
    uint8_t *end() {
        return start + size;
    }
};

class drawingdisplay {
public:
    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
    virtual int GetBitsPerPixel() const = 0;
    virtual framebuffer_line Line(int x, int y, int width) const = 0;

    void Draw8x8Char(int x, int y, uint32_t color, uint32_t background, uint64_t bitmap);
    void ShiftUpLines(int lines, uint32_t color);
};

#endif //JEOKERNEL_DRAWINGDISPLAY_H
