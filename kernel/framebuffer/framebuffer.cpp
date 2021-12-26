//
// Created by sigsegv on 12/25/21.
//

#include <framebuffer/framebuffer.h>
#include <thread>

framebuffer::framebuffer(const MultibootFramebuffer &info) : vm(nullptr), frames(nullptr), pitch(info.framebuffer_pitch), width(info.framebuffer_width), height(info.framebuffer_height), bpp(info.framebuffer_bpp) {
    uint64_t physaddr{info.framebuffer_addr};
    uint64_t size{pitch};
    size *= height;
    vm = new vmem{size};
    {
        uint64_t end{physaddr + size};
        {
            uint64_t addr{physaddr};
            uint32_t page{0};
            while (addr < end) {
                update_pageentr(addr, [](pageentr &pe) -> void {
                    pe.os_phys_avail = 0;
                });
                vm->page(page).rwmap(addr, true, false);
                page++;
                addr += 0x1000;
            }
        }
    }
    frames = (uint8_t *) vm->pointer();
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
