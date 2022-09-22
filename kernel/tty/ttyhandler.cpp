//
// Created by sigsegv on 9/22/22.
//

#include "ttyhandler.h"
#include "dev/ttydev.h"

ttyhandler::ttyhandler() {
}

std::shared_ptr<ttyhandler> ttyhandler::Create(const std::string &name) {
    std::shared_ptr<ttyhandler> handler{new ttyhandler};
    std::shared_ptr<ttydev> ttyd = ttydev::Create(handler, name);
    return handler;
}