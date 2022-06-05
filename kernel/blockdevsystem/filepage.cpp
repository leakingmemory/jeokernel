//
// Created by sigsegv on 6/5/22.
//

#include <files/filepage.h>
#include <kfs/filepage_data.h>
#include <pagealloc.h>
#include <klogger.h>
#include <core/vmem.h>

#define FP_PAGESIZE FILEPAGE_PAGE_SIZE

class filepage_pointer_impl : public filepage_pointer {
private:
    vmem vm;
public:
    filepage_pointer_impl(uintptr_t addr) : vm(FP_PAGESIZE) {
        vm.page(0).rwmap(addr);
    }
    void *Pointer() override;
};

void *filepage_pointer_impl::Pointer() {
    return vm.pointer();
}

filepage::filepage() : page(new filepage_data) {}

std::shared_ptr<filepage_pointer> filepage::Pointer() {
    if (page->physpage != 0) {
        return std::make_shared<filepage_pointer_impl>(page->physpage);
    } else {
        return {};
    }
}

std::shared_ptr<filepage_data> filepage::Raw() {
    return page;
}

filepage_data::filepage_data() : physpage(0) {
    physpage = ppagealloc(FP_PAGESIZE);
    if (physpage == 0) {
        wild_panic("Out of phys pages (filepage_data)");
    }
}

filepage_data::~filepage_data() {
    if (physpage != 0) {
        ppagefree(physpage, FP_PAGESIZE);
        physpage = 0;
    }
}
