//
// Created by sigsegv on 5/25/22.
//

#include <files/filepage.h>

class filepage_pointer_impl;

class filepage_data {
    friend filepage_pointer_impl;
private:
    void *ptr;
public:
    filepage_data();
    filepage_data(const filepage_data &) = delete;
    filepage_data(filepage_data &&) = delete;
    filepage_data &operator =(const filepage_data &) = delete;
    filepage_data &operator =(filepage_data &&) = delete;
    ~filepage_data();
};

filepage_data::filepage_data() : ptr(nullptr) {
    ptr = malloc(FILEPAGE_PAGE_SIZE);
}

filepage_data::~filepage_data() {
    if (ptr != nullptr) {
        free(ptr);
        ptr = nullptr;
    }
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
    void *Pointer() override;
};

void *filepage_pointer_impl::Pointer() {
    if (page) {
        return page->ptr;
    }
    return nullptr;
}

filepage::filepage() : page(new filepage_data()) {
}

std::shared_ptr<filepage_pointer> filepage::Pointer() {
    return std::make_shared<filepage_pointer_impl>(page);
}

std::shared_ptr<filepage_data> filepage::Raw() {
    return page;
}
