//
// Created by sigsegv on 11/11/23.
//

#ifndef JEOKERNEL_REFERENCE_H
#define JEOKERNEL_REFERENCE_H

#include <memory>

template <class T> class resource;
class reference_dynamic_cast_impl_helper;
class referrer;

class reference_typeerasure {
protected:
    reference_typeerasure() {}
public:
    reference_typeerasure(const reference_typeerasure &) = delete;
    reference_typeerasure(reference_typeerasure &&) = delete;
    reference_typeerasure &operator =(const reference_typeerasure &) = delete;
    reference_typeerasure &operator =(reference_typeerasure &&) = delete;
    virtual ~reference_typeerasure() = default;
    virtual void *GetResource() = 0;
    virtual void Forget() = 0;
    [[nodiscard]] virtual std::shared_ptr<reference_typeerasure> Clone(const std::shared_ptr<referrer> &) const = 0;
};

template <class T> class reference;

template <class T> class reference_reference : public reference_typeerasure {
    friend resource<T>;
    friend reference<T>;
private:
    std::shared_ptr<resource<T>> ref{};
    uint64_t id{0};
    reference_reference() = delete;
public:
    explicit reference_reference(const std::shared_ptr<resource<T>> &ref, uint64_t id) : ref(ref), id(id) {}
    void *GetResource() override {
        return ref->GetResource();
    }
    void Forget() override {
        ref->Forget(id);
    }
    [[nodiscard]] std::shared_ptr<reference_typeerasure> Clone(const std::shared_ptr<referrer> &referrer) const override {
        std::shared_ptr<resource<T>> resource = ref;
        std::shared_ptr<reference_reference<T>> reference = std::make_shared<reference_reference<T>>(resource, ref->NewRef(referrer));
        return reference;
    }
};

template <class T> class reference {
    friend resource<T>;
    friend reference_dynamic_cast_impl_helper;
    template <class> friend class reference;
private:
    std::shared_ptr<reference_typeerasure> ref;
    T *ptr;
    reference(const std::shared_ptr<resource<T>> &ref, uint64_t id) : ref(), ptr(ref->GetResource()) {
        this->ref = std::make_shared<reference_reference<T>>(ref, id);
    }
public:
    reference() : ref(), ptr(nullptr) {}
    reference(const reference &cp) = delete;
    reference(reference &&mv) = default;
    template <class P> reference(reference<P> &&mv) : ref(std::move(mv.ref)), ptr() {
        ptr = mv.ptr != nullptr ? mv.ptr : nullptr;
        mv.ref = {};
        mv.ptr = nullptr;
    }
    reference &operator = (const reference &) = delete;
    reference &operator = (reference &&mv) {
        if (this == &mv) {
            return *this;
        }
        if (ref) {
            ref->Forget();
        }
        ref = std::move(mv.ref);
        ptr = mv.ptr;
        mv.ref = {};
        mv.ptr = nullptr;
        return *this;
    }
    ~reference() {
        if (ref) {
            ref->Forget();
            ref = {};
            ptr = nullptr;
        }
    }
    T &operator * () const {
        return *ptr;
    }
    T *operator -> () const {
        return ptr;
    }
    T *operator -> () {
        return ptr;
    }
    explicit operator bool () const {
        return ptr != nullptr;
    }
    reference_typeerasure &GetResourceRef() {
        return *ref;
    }
    std::shared_ptr<resource<T>> GetResource() {
        std::shared_ptr<reference_reference<T>> ref = std::dynamic_pointer_cast<reference_reference<T>>(this->ref);
        if (ref) {
            return ref->ref;
        } else {
            return {};
        }
    }
    reference<T> CreateReference(const std::shared_ptr<class referrer> &referrer) const {
        reference<T> newRef{};
        newRef.ref = ref->Clone(referrer);
        newRef.ptr = ptr;
        return newRef;
    }
};

template <class T> struct reference_data {
    std::shared_ptr<reference_typeerasure> ref;
    T *ptr;
};

class reference_dynamic_cast_impl_helper {
protected:
    template <class T> reference_data<T> TakeItAll(reference<T> &&ref) {
        reference_data<T> data{
                .ref = ref.ref,
                .ptr = ref.ptr,
        };
        ref.ref = {};
        ref.ptr = nullptr;
        return data;
    }
    template <class T> void SetResource(reference<T> &refobj, const std::shared_ptr<reference_typeerasure> &ref, T *ptr) {
        refobj.ref = ref;
        refobj.ptr = ptr;
    }
};

template <class T, class P> class reference_dynamic_cast_impl : reference_dynamic_cast_impl_helper {
private:
    reference<T> refResult{};
public:
    reference_dynamic_cast_impl() = delete;
    explicit reference_dynamic_cast_impl(reference<P> &&reference) {
        auto orig = TakeItAll(std::move(reference));
        T *ptr = dynamic_cast<T *>(orig.ptr);
        if (ptr != nullptr) {
            SetResource(refResult, orig.ref, ptr);
        } else if (orig.ref) {
            orig.ref->Forget();
        }
    }
    reference<T> GetResult() {
        reference<T> result{std::move(refResult)};
        return result;
    }
};

template <class T, class P> bool reference_is_a(const reference<P> &ref) {
    P *ptr = &(*ref);
    if (ptr != nullptr && dynamic_cast<T*>(ptr) != nullptr) {
        return true;
    } else {
        return false;
    }
}

template <class T, class P> reference<T> reference_dynamic_cast(reference<P> &&ref) {
    reference_dynamic_cast_impl<T,P> cast{std::move(ref)};
    return cast.GetResult();
}

#endif //JEOKERNEL_REFERENCE_H
