//
// Created by sigsegv on 7/7/21.
//

#include <time.h>
#include <core/scheduler.h>

extern "C" {
    time_t time(time_t *tloc) {
        time_t tm = (time_t) get_ticks() / 100;
        if (tloc != nullptr) {
            *tloc = tm;
        }
        return tm;
    }

    char *ctime(const time_t *timep) {
        return (char *) "Not impl ctime";
    }
}