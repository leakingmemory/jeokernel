//
// Created by sigsegv on 12/26/21.
//

#include <framebuffer/drawingdisplay.h>
#include <cstdint>
#include <string>

void drawingdisplay::Draw8x8Char(int x, int y, uint32_t color, uint32_t background, uint64_t bitmap) {
    if (x < 0 || y < 0) {
        return;
    }
    uint8_t bits = GetBitsPerPixel();
    int height = GetHeight();
    int width = GetWidth();
    int chw = 8;
    if ((x + chw) >= width) {
        if (x >= width) {
            return;
        }
        chw = width - x;
    }
    if (bits >= 8) {
        uint8_t bytes = bits >> 3;
        for (int dy = 0; dy < 8; dy++) {
            uint8_t bm_ln = (bitmap & 0xFF);
            bitmap = bitmap >> 8;
            if ((y + dy) >= height) {
                break;
            }
            auto line = Line(x, y + dy, 8);
            for (int dx = 0; dx < chw; dx++) {
                uint32_t c{color};
                uint32_t b{background};
                for (int i = 0; i < bytes; i++) {
                    *(line.start) = (bm_ln & 1) != 0 ? (c & 0xFF) : (b & 0xFF);
                    ++line.start;
                    c = c >> 8;
                    b = b >> 8;
                }
                bm_ln = bm_ln >> 1;
            }
        }
    }
}

void drawingdisplay::ShiftUpLines(int lines, uint32_t color) {
    int width{GetWidth()};
    int end{GetHeight() - lines};
    int src_y = lines;
    for (int y = 0; y < end; y++, src_y++) {
        auto source = Line(0, src_y, width);
        auto dest = Line(0, y, width);
        memmove(dest.start, source.start, source.size);
    }
    uint8_t bits = GetBitsPerPixel();
    if (bits >= 8) {
        uint8_t bytes = bits >> 3;
        for (int y = 0; y < lines; y++) {
            auto line = Line(0, y + end, width);
            uint8_t *ptr = line.start;
            uint8_t *end = line.end() - (bytes - 1);
            while (ptr < end) {
                uint32_t c{color};
                for (uint8_t i = 0; i < bytes; i++) {
                    *(ptr) = (c & 0xFF);
                    c = c >> 8;
                    ++ptr;
                }
            }
        }
    }
}
