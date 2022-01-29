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

[[noreturn]] void kshell::run() {
    std::string input{};
    while(true) {
        std::shared_ptr<keyboard_line_consumer> consumer{new keyboard_line_consumer()};
        get_klogger() << "# ";
        Keyboard().consume(consumer);
        keyboard_blocking_string *stringGetter = consumer->GetBlockingString();
        input.clear();
        input.append("\n");
        {
            const std::string &keyboardString = stringGetter->GetString();
            input.append(keyboardString);
        }
        input.append("\n");
        get_klogger() << input.c_str();
    }
}

