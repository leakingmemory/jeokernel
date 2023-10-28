//
// Created by sigsegv on 6/5/22.
//

#ifndef JEOKERNEL_FILEPAGE_DATA_H
#define JEOKERNEL_FILEPAGE_DATA_H

#include <cstdint>

class kfile;
class filepage;
class filepage_ref;

class filepage_data {
    friend filepage;
    friend filepage_ref;
    friend kfile;
private:
    uintptr_t physpage;
    uint32_t ref;
    uint32_t initRef;
    uint32_t dirty;
public:
    filepage_data();
    ~filepage_data();
    filepage_data(const filepage_data &) = delete;
    filepage_data(filepage_data &&) = delete;
    filepage_data &operator =(const filepage_data &) = delete;
    filepage_data &operator =(filepage_data &&) = delete;
private:
    void up();
    void down();
    void initDone();
    void setDirty(uint32_t dirty);
    uint32_t getDirty();
    uint32_t getDirtyAndClear();
};

class filepage_ref {
private:
    std::shared_ptr<filepage_data> data;
public:
    filepage_ref() : data() {
    }
    filepage_ref(std::shared_ptr<filepage_data> data) : data(data) {
        if (data) {
            data->up();
        }
    }
    ~filepage_ref() {
        if (data) {
            data->down();
            data = {};
        }
    }
    filepage_ref(const filepage_ref &cp) : data(cp.data) {
        if (data) {
            data->up();
        }
    }
    filepage_ref(filepage_ref &&mv) : data(mv.data) {
        if (this != &mv) {
            mv.data = {};
        }
    }
private:
    void swap(filepage_ref &other) {
       auto data = other.data;
       other.data = this->data;
       this->data = data;
    }
public:
    filepage_ref &operator =(const filepage_ref &cp) {
        filepage_ref loc{cp};
        swap(loc);
        return *this;
    }
    filepage_ref &operator =(filepage_ref &&mv) {
        auto olddata = this->data;
        this->data = mv.data;
        if (this != &mv) {
            mv.data = {};
            if (olddata) {
                olddata->down();
            }
        }
        return *this;
    }

    uintptr_t PhysAddr() {
        return data ? data->physpage : 0;
    }

    operator bool() const {
        if (data) {
            return true;
        } else {
            return false;
        }
    }
};

#endif //JEOKERNEL_FILEPAGE_DATA_H
