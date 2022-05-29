//
// Created by sigsegv on 5/25/22.
//

#ifndef FSBITS_FILEPAGE_H
#define FSBITS_FILEPAGE_H

#include <cstdint>
#include <memory>

#define FILEPAGE_PAGE_SIZE  4096

class filepage_data;
class filepage_raw;

class filepage_pointer {
public:
    virtual void *Pointer() = 0;
};

class filepage {
private:
    std::shared_ptr<filepage_data> page;
public:
    filepage();
    std::shared_ptr<filepage_pointer> Pointer();
    std::shared_ptr<filepage_data> Raw();
};

#endif //FSBITS_FILEPAGE_H
