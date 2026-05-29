//
// Created by sigsegv on 5/28/26.
//

#include "kshell_stream.h"
#include <kshell/kshell.h>
#include <tty/tty.h>

kshell_stream &kshell_stream::write(const char *s, std::streamsize count) {
    sh->Tty()->Write(s, count);
    return *this;
}
