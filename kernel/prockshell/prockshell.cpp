//
// Created by sigsegv on 12/2/23.
//

#include <prockshell/prockshell.h>
#include <kshell/kshell.h>
#include "kshell_ups.h"

prockshell::prockshell(kshell &shell) {
    shell.AddCommand(std::make_shared<kshell_ups>());
}
