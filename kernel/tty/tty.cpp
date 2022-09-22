//
// Created by sigsegv on 9/22/22.
//

#include "ttyhandler.h"
#include <tty/ttyinit.h>

void InitTty() {
    ttyhandler::Create("tty");
}
