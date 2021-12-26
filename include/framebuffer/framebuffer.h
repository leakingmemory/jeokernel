//
// Created by sigsegv on 12/25/21.
//

#ifndef JEOKERNEL_FRAMEBUFFER_H
#define JEOKERNEL_FRAMEBUFFER_H

#include <multiboot.h>
#include "drawingdisplay.h"

class framebuffer : public drawingdisplay {
private:
    vmem *vm;
    uint8_t *frames;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
public:
    framebuffer(const MultibootFramebuffer &info);
    ~framebuffer();
    int GetWidth() const override;
    int GetHeight() const override;
    int GetBitsPerPixel() const override;
    framebuffer_line Line(int x, int y, int width) const override;
};

#endif //JEOKERNEL_FRAMEBUFFER_H
