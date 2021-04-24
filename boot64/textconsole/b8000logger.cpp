//
// Created by sigsegv on 19.04.2021.
//

#include "b8000logger.h"

b8000logger::b8000logger() : KLogger(), cons() {
}

b8000logger::~b8000logger() noexcept {
}

b8000logger & b8000logger::operator << (const char *str) {
    cons << str;
    return *this;
}
