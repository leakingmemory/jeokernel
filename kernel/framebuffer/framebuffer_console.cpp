//
// Created by sigsegv on 12/26/21.
//

#include "framebuffer_console.h"

framebuffer_console::framebuffer_console(drawingdisplay &display) : display(display), width(display.GetWidth() >> 3), height(display.GetHeight() >> 3), pos(0), chsize(width * height) {
}
