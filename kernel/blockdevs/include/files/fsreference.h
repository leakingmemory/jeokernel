//
// Created by sigsegv on 11/5/23.
//

#ifndef JEOKERNEL_FSREFERENCE_H
#define JEOKERNEL_FSREFERENCE_H

#include <memory>

template <class T> class fsresource;

template <class T> class fsreference {
    friend fsresource<T>;
private:
    std::shared_ptr<fsresource<T>> ref{};
    T *ptr;
    uint64_t id{0};
    fsreference(const std::shared_ptr<fsresource<T>> &ref, uint64_t id) : ref(ref), ptr(ref->GetResource()), id(id) {}
public:
    fsreference() = default;
    fsreference(fsreference &mv) = default;
    fsreference &operator = (const fsreference &) = delete;
    fsreference &operator = (fsreference &&mv) {
        if (this == &mv) {
            return *this;
        }
        if (ref) {
            ref->Forget(id);
        }
        ref = std::move(mv.ref);
        id = mv.id;
        ptr = mv.ptr;
        mv.ref = {};
        mv.id = 0;
        mv.ptr = nullptr;
        return *this;
    }
    ~fsreference() {
        if (ref) {
            ref->Forget(id);
            ref = {};
        }
    }
    T &operator * () const {
        return *ptr;
    }
    const T *operator -> () const {
        return ptr;
    }
    T *operator -> () {
        return ptr;
    }
    fsresource<T> &GetFsResourceRef() {
        return *ref;
    }
    std::shared_ptr<fsresource<T>> GetFsResource() {
        return ref;
    }
};

#endif //JEOKERNEL_FSREFERENCE_H
