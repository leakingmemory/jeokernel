//
// Created by sigsegv on 7/6/21.
//

#include <stdint.h>
#include <thread>
#include <signal.h>
#include <dlfcn.h>

extern "C" {
    uint32_t errno = 0;

    void exit(int32_t code) {
        wild_panic("called exit(..) in kernel");
    }

    sighandler_t signal(int signal, sighandler_t handler) {
        wild_panic("using signal(..) in kernel");
        return nullptr;
    }

    int stat(const char *pathname, struct stat *statbuf) {
        return -1;
    }

    int isatty(int fd) {
        if (fd == 0) {
            return 1;
        } else {
            return 0;
        }
    }

    int dladdr(const void *addr, Dl_info *info) {
        return -1;
    }
}