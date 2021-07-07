//
// Created by sigsegv on 7/1/21.
//

#ifndef JEOKERNEL_STAT_H
#define JEOKERNEL_STAT_H

/*
 * Stub to compile acpica
 */

#ifdef __cplusplus
extern "C" {
#endif

    struct stat {
    };

    int stat(const char *pathname, struct stat *statbuf);

#ifdef __cplusplus
};
#endif

#endif //JEOKERNEL_STAT_H
