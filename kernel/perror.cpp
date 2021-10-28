//
// Created by sigsegv on 7/7/21.
//

#include <stdio.h>
#include <sstream>
#include <klogger.h>

extern "C" {
    void perror(const char *s) {
        std::stringstream ss{};
        ss << s << ": Unknown error (perror N/A)\n";
        get_klogger() << ss.str().c_str();
    }
}
