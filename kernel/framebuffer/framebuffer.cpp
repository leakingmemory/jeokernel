//
// Created by sigsegv on 12/25/21.
//

#include <framebuffer/framebuffer.h>
#include <thread>
#include <physpagemap.h>

framebuffer::framebuffer(phys_t base_addr, uintptr_t pitch, uint32_t width, uint32_t height, uint8_t bpp) : vm(nullptr), frames(nullptr), pitch(pitch), width(width), height(height), bpp(bpp) {
    uint64_t physaddr{base_addr};
    uintptr_t size{pitch * height};
    vm = new vmem{size};
    {
        uint64_t end{physaddr + size};
        {
            uint64_t addr{physaddr};
            uint32_t page{0};
            auto *phys = get_physpagemap();
            while (addr < end) {
                if ((addr >> 12) < phys->max()) {
                    phys->claim(addr >> 12);
                }
                vm->page(page).rwmap(addr, true, false);
                page++;
                addr += 0x1000;
            }
        }
    }
    frames = (uint8_t *) vm->pointer();
}

framebuffer::framebuffer(const MultibootFramebuffer &info) : framebuffer(info.framebuffer_addr, info.framebuffer_pitch, info.framebuffer_width, info.framebuffer_height, info.framebuffer_bpp) {
}

framebuffer::~framebuffer() {
    delete vm;
}

framebuffer_line framebuffer::Line(int x, int y, int width) const {
    unsigned int pos = y;
    pos = pos * pitch;
    unsigned int end = pos;
    {
        unsigned int xpos = x;
        {
            unsigned int endp{xpos + width};
            endp = endp * bpp;
            endp = endp >> 3;
            end += endp;
        }
        xpos = xpos * bpp;
        xpos = xpos >> 3;
        pos += xpos;
    }
    framebuffer_line ln{
        .start = frames + pos,
        .size = end - pos
    };
    return ln;
}

int framebuffer::GetWidth() const {
    return (int) width;
}

int framebuffer::GetHeight() const {
    return (int) height;
}

int framebuffer::GetBitsPerPixel() const {
    return bpp;
}
