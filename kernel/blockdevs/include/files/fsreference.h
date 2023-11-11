//
// Created by sigsegv on 11/5/23.
//

#ifndef JEOKERNEL_FSREFERENCE_H
#define JEOKERNEL_FSREFERENCE_H

#include <memory>

template <class T> class fsresource;
class fsreference_dynamic_cast_impl_helper;
class fsreferrer;

class fsreference_typeerasure {
protected:
    fsreference_typeerasure() {}
public:
    fsreference_typeerasure(const fsreference_typeerasure &) = delete;
    fsreference_typeerasure(fsreference_typeerasure &&) = delete;
    fsreference_typeerasure &operator =(const fsreference_typeerasure &) = delete;
    fsreference_typeerasure &operator =(fsreference_typeerasure &&) = delete;
    virtual ~fsreference_typeerasure() = default;
    virtual void *GetResource() = 0;
    virtual void Forget() = 0;
    [[nodiscard]] virtual std::shared_ptr<fsreference_typeerasure> Clone(const std::shared_ptr<fsreferrer> &) const = 0;
};

template <class T> class fsreference;

template <class T> class fsreference_reference : public fsreference_typeerasure {
    friend fsresource<T>;
    friend fsreference<T>;
private:
    std::shared_ptr<fsresource<T>> ref{};
    uint64_t id{0};
    fsreference_reference() = delete;
public:
    explicit fsreference_reference(const std::shared_ptr<fsresource<T>> &ref, uint64_t id) : ref(ref), id(id) {}
    void *GetResource() override {
        return ref->GetResource();
    }
    void Forget() override {
        ref->Forget(id);
    }
    [[nodiscard]] std::shared_ptr<fsreference_typeerasure> Clone(const std::shared_ptr<fsreferrer> &referrer) const override {
        std::shared_ptr<fsresource<T>> resource = ref;
        std::shared_ptr<fsreference_reference<T>> reference = std::make_shared<fsreference_reference<T>>(resource, ref->NewRef(referrer));
        return reference;
    }
};

template <class T> class fsreference {
    friend fsresource<T>;
    friend fsreference_dynamic_cast_impl_helper;
    template <class> friend class fsreference;
private:
    std::shared_ptr<fsreference_typeerasure> ref;
    T *ptr;
    fsreference(const std::shared_ptr<fsresource<T>> &ref, uint64_t id) : ref(), ptr(ref->GetResource()) {
        this->ref = std::make_shared<fsreference_reference<T>>(ref, id);
    }
public:
    fsreference() : ref(), ptr(nullptr) {}
    fsreference(const fsreference &cp) = delete;
    fsreference(fsreference &&mv) = default;
    template <class P> fsreference(fsreference<P> &&mv) : ref(std::move(mv.ref)), ptr() {
        ptr = mv.ptr != nullptr ? mv.ptr : nullptr;
        mv.ref = {};
        mv.ptr = nullptr;
    }
    fsreference &operator = (const fsreference &) = delete;
    fsreference &operator = (fsreference &&mv) {
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
    ~fsreference() {
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
    fsreference_typeerasure &GetFsResourceRef() {
        return *ref;
    }
    std::shared_ptr<fsresource<T>> GetFsResource() {
        std::shared_ptr<fsreference_reference<T>> ref = std::dynamic_pointer_cast<fsreference_reference<T>>(this->ref);
        if (ref) {
            return ref->ref;
        } else {
            return {};
        }
    }
    fsreference<T> CreateReference(const std::shared_ptr<fsreferrer> &referrer) const {
        fsreference<T> newRef{};
        newRef.ref = ref->Clone(referrer);
        newRef.ptr = ptr;
        return newRef;
    }
};

template <class T> struct fsreference_data {
    std::shared_ptr<fsreference_typeerasure> ref;
    T *ptr;
};

class fsreference_dynamic_cast_impl_helper {
protected:
    template <class T> fsreference_data<T> TakeItAll(fsreference<T> &&ref) {
        fsreference_data<T> data{
            .ref = ref.ref,
            .ptr = ref.ptr,
        };
        ref.ref = {};
        ref.ptr = nullptr;
        return data;
    }
    template <class T> void SetResource(fsreference<T> &refobj, const std::shared_ptr<fsreference_typeerasure> &ref, T *ptr) {
        refobj.ref = ref;
        refobj.ptr = ptr;
    }
};

template <class T, class P> class fsreference_dynamic_cast_impl : fsreference_dynamic_cast_impl_helper {
private:
    fsreference<T> refResult{};
public:
    fsreference_dynamic_cast_impl() = delete;
    explicit fsreference_dynamic_cast_impl(fsreference<P> &&reference) {
        auto orig = TakeItAll(std::move(reference));
        T *ptr = dynamic_cast<T *>(orig.ptr);
        if (ptr != nullptr) {
            SetResource(refResult, orig.ref, ptr);
        } else if (orig.ref) {
            orig.ref->Forget();
        }
    }
    fsreference<T> GetResult() {
        fsreference<T> result{std::move(refResult)};
        return result;
    }
};

template <class T, class P> fsreference<T> fsreference_dynamic_cast(fsreference<P> &&ref) {
    fsreference_dynamic_cast_impl<T,P> cast{std::move(ref)};
    return cast.GetResult();
}

#endif //JEOKERNEL_FSREFERENCE_H
