//
// Created by sigsegv on 6/13/21.
//

#include "Phys32Page.h"
#include <pagealloc.h>
#include <klogger.h>

Phys32Page::Phys32Page(size_t size) : vm(size), size(size) {
    physaddr = ppagealloc32(size);
    if (physaddr == 0) {
        wild_panic("Unable to allocate 32bit memory");
    }
    for (size_t i = 0; i < vm.npages(); i++) {
        vm.page(i).rwmap(physaddr + (i << 12));
    }
}

Phys32Page::~Phys32Page() {
    ppagefree(physaddr, size);
}
