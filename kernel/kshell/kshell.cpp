//
// Created by sigsegv on 1/27/22.
//

#include "kshell.h"
#include "keyboard/keyboard.h"

kshell::kshell() : mtx(), exit(false), shell([this] () { run(); }) {
}

kshell::~kshell() {
    {
        std::lock_guard lock{mtx};
        exit = true;
    }
    shell.join();
}

void kshell::run() {
    Keyboard().consume(std::make_shared<keyboard_line_consumer>());
}

