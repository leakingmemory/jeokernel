//
// Created by sigsegv on 12/26/21.
//

#include <string>
#include "framebuffer_console.h"

framebuffer_console::framebuffer_console(drawingdisplay &display) :
    display(display), cursorRestore(), cursorWidth(0), cursorHeight(0), cursorX(0), cursorY(0),
    width(display.GetWidth() >> 3), height(display.GetHeight() >> 3), pos(0), chsize(width * height) {
}

void framebuffer_console::SetCursorVisibility(bool visibility, uint32_t color) {
    if (visibility && cursorRestoreSize == 0) {
        cursorY = pos / width;
        cursorX = pos % width;
        cursorX = cursorX << 3;
        cursorY = (cursorY << 3) + 6;
        cursorWidth = 8;
        cursorHeight = 2;
        auto line1 = display.Line(cursorX, cursorY, cursorWidth);
        cursorRestoreSize = line1.size << 1;
        if (cursorRestoreSize <= sizeof(cursorRestore)) {
            auto line2 = display.Line(cursorX, cursorY + 1, cursorWidth);
            if (line1.size == line2.size) {
                uint8_t *restore = &(cursorRestore[0]);
                memmove(restore, line1.start, line1.size);
                restore += line1.size;
                memmove(restore, line2.start, line2.size);
                unsigned int bytesperpixel = line1.size / cursorWidth;
                if (bytesperpixel >= 1) {
                    unsigned int pos = 0;
                    for (unsigned int x = 0; x < cursorWidth; x++) {
                        uint32_t c{color};
                        for (unsigned int i = 0; i < bytesperpixel; i++) {
                            if (pos < line1.size && pos < line2.size) {
                                *(line1.start + pos) = (c & 0xFF);
                                *(line2.start + pos) = (c & 0xFF);
                            }
                            c = c >> 8;
                            ++pos;
                        }
                    }
                }
            } else {
                cursorRestoreSize = 0;
            }
        } else {
            cursorRestoreSize = 0;
        }
    } else if (!visibility && cursorRestoreSize != 0) {
        uint8_t *restore = &(cursorRestore[0]);
        uint32_t restoreSize = cursorRestoreSize;
        for (unsigned int y = 0; y < cursorHeight; y++) {
            auto line = display.Line(cursorX, cursorY + y, cursorWidth);
            if (line.size > restoreSize) {
                break;
            }
            memmove(line.start, restore, line.size);
            restore += line.size;
            restoreSize -= line.size;
        }
        cursorRestoreSize = 0;
    }
}
