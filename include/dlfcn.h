//
// Created by sigsegv on 4/28/22.
//

#ifndef JEOKERNEL_DLFCN_H
#define JEOKERNEL_DLFCN_H

typedef struct {
    const char *dli_fname;
    void       *dli_fbase;
    const char *dli_sname;
    void       *dli_saddr;
} Dl_info;

#ifdef __cplusplus
extern "C" {
#endif
    int dladdr(const void *addr, Dl_info *info);
#ifdef __cplusplus
};
#endif

#endif //JEOKERNEL_DLFCN_H
