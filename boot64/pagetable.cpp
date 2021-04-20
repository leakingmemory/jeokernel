//
// Created by sigsegv on 20.04.2021.
//

#include <stdint.h>
#include <pagetable_impl.h>

pagetable &get_pml4t() {
    return *((pagetable *) 0x1000);
}
