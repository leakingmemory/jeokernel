//
// Created by sigsegv on 6/5/22.
//

#include <klogger.h>

#define NUM_DESTRUCTORS 128

static void (*dest_func[NUM_DESTRUCTORS]) (void *);
static void *dest_arg[NUM_DESTRUCTORS];
static int numDestructors = 0;

extern "C" {
    int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle) {
        if (dso_handle != nullptr) {
            wild_panic("Dynamic linking is not yet supported");
        }
        if (numDestructors < NUM_DESTRUCTORS) {
            dest_func[numDestructors] = func;
            dest_arg[numDestructors] = arg;
            ++numDestructors;
        } else {
            wild_panic("Max static destructors exceeded, extend dynlinker NUM_DESTRUCTORS");
        }
        return 0;
    }
}