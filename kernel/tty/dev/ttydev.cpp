//
// Created by sigsegv on 9/22/22.
//

#include "ttydev.h"
#include "ttyfsdev.h"
#include <devfs/devfs.h>

ttydev::ttydev(std::shared_ptr<ttyhandler> handler) : handler(handler) {
}

std::shared_ptr<ttydev> ttydev::Create(std::shared_ptr<ttyhandler> handler, const std::string &name) {
    std::shared_ptr<ttydev> ttyd{new ttydev(handler)};
    std::shared_ptr<ttyfsdev> fsdev = ttyfsdev::Create(ttyd);
    GetDevfs()->GetRoot()->Add(name, fsdev);
    return ttyd;
}