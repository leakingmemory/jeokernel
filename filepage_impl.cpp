//
// Created by sigsegv on 5/25/22.
//

#include <files/filepage.h>
#include <cstring>

class filepage_pointer_impl;

class filepage_data {
    friend filepage_pointer_impl;
private:
    void *ptr;
    size_t dirty;
public:
    filepage_data();
    filepage_data(const filepage_data &) = delete;
    filepage_data(filepage_data &&) = delete;
    filepage_data &operator =(const filepage_data &) = delete;
    filepage_data &operator =(filepage_data &&) = delete;
    ~filepage_data();
    void SetDirty(size_t length);
    void Zero();
    bool IsDirty() const;
    size_t GetDirtyLength() const;
};

filepage_data::filepage_data() : ptr(nullptr), dirty(0) {
    ptr = malloc(FILEPAGE_PAGE_SIZE);
}

filepage_data::~filepage_data() {
    if (ptr != nullptr) {
        free(ptr);
        ptr = nullptr;
    }
}

void filepage_data::Zero() {
    if (ptr != nullptr) {
        bzero(ptr, FILEPAGE_PAGE_SIZE);
    }
}

void filepage_data::SetDirty(size_t length) {
    if (length > dirty) {
        dirty = length;
    }
}

bool filepage_data::IsDirty() const {
    return dirty > 0;
}

size_t filepage_data::GetDirtyLength() const {
    return dirty;
}

class filepage_pointer_impl : public filepage_pointer {
private:
    std::shared_ptr<filepage_data> page;
public:
    filepage_pointer_impl(std::shared_ptr<filepage_data> data) : page(data) {
    }
    filepage_pointer_impl(const filepage_pointer_impl &) = delete;
    filepage_pointer_impl(filepage_pointer_impl &&) = delete;
    filepage_pointer_impl &operator =(const filepage_pointer_impl &) = delete;
    filepage_pointer_impl &operator =(filepage_pointer_impl &&) = delete;
    void *Pointer() const override;
    void SetDirty(size_t length) override;
};

void *filepage_pointer_impl::Pointer() const {
    if (page) {
        return page->ptr;
    }
    return nullptr;
}

void filepage_pointer_impl::SetDirty(size_t length) {
    if (page) {
        page->SetDirty(length);
    }
}

filepage::filepage() : page(new filepage_data()) {
}

std::shared_ptr<filepage_pointer> filepage::Pointer() {
    return std::make_shared<filepage_pointer_impl>(page);
}

std::shared_ptr<filepage_data> filepage::Raw() {
    return page;
}

void filepage::Zero() {
    page->Zero();
}

void filepage::SetDirty(size_t length) {
    page->SetDirty(length);
}

bool filepage::IsDirty() const {
    return page && page->IsDirty();
}

size_t filepage::GetDirtyLength() const {
    return page ? page->GetDirtyLength() : 0;
}
