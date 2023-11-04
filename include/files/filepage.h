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
    virtual void *Pointer() const = 0;
    virtual void SetDirty(size_t length) = 0;
};

class filepage {
private:
    std::shared_ptr<filepage_data> page;
public:
    filepage();
    std::shared_ptr<filepage_pointer> Pointer();
    std::shared_ptr<filepage_data> Raw();
    void Zero();
    void SetDirty(size_t length);
    bool IsDirty() const;
    size_t GetDirtyLength() const;
    size_t GetDirtyLengthAndClear();
};

struct dirty_block_fsref {
};

struct dirty_block {
    std::shared_ptr<dirty_block_fsref> fsref{};
    std::shared_ptr<filepage> page1{};
    std::shared_ptr<filepage> page2{};
    uint64_t blockaddr;
    uint32_t offset, length;
};

#endif //FSBITS_FILEPAGE_H
