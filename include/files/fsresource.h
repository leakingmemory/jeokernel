//
// Created by sigsegv on 11/5/23.
//

#ifndef JEOKERNEL_FSRESOURCE_H
#define JEOKERNEL_FSRESOURCE_H

#include <memory>
#include <vector>
#include <mutex>
#include <iostream>
#include "fsreference.h"
#include "fsreferrer.h"

class fsreferrer;

struct rawfsreference {
    std::weak_ptr<fsreferrer> referrer;
    uint64_t id;
};

class fsresourcelock {
public:
    virtual void lock() = 0;
    virtual void unlock() = 0;
};

struct rawfsresource {
    std::shared_ptr<fsresourcelock> mtx{};
    std::vector<rawfsreference> refs{};
    uint64_t serial{0};
    void Forget(uint64_t id);
    uint64_t NewRef(const std::shared_ptr<fsreferrer> &);
};

class fsresourcelockfactory {
public:
    virtual std::shared_ptr<fsresourcelock> Create() = 0;
};

class fsresourceunwinder {
public:
    virtual void PrintTraceBack(int indent) = 0;
};

template <class T> class fsresource : private rawfsresource, public fsresourceunwinder {
    friend fsreference<T>;
    friend fsreference_reference<T>;
private:
    std::weak_ptr<fsresource<T>> self_ref{};
protected:
    void SetSelfRef(const std::shared_ptr<fsresource<T>> &ref) {
        std::weak_ptr<fsresource<T>> w{ref};
        self_ref = w;
    }
    virtual T *GetResource() = 0;
public:
    fsresource() = delete;
    fsresource(fsresourcelockfactory &lockfactory) {
        mtx = lockfactory.Create();
    }
    fsresource(const fsresource &) = delete;
    fsresource(fsresource &&) = delete;
    fsresource &operator =(const fsresource &) = delete;
    fsresource &operator =(fsresource &&) = delete;
    fsreference<T> CreateReference(const std::shared_ptr<fsreferrer> &referrer) {
        std::weak_ptr<fsreferrer> w{referrer};
        uint64_t id;
        {
            std::lock_guard lock{*mtx};
            rawfsreference &ref = refs.emplace_back();
            ref.referrer = w;
            ref.id = ++serial;
            id = ref.id;
        }
        fsreference<T> ref{};
        ref.ref = std::make_shared<fsreference_reference<T>>(self_ref.lock(), id);
        ref.ptr = (T *) ref.ref->GetResource();
        return ref;
    }
    void PrintTraceBack(int indent) override {
        std::string indentStr{};
        for (int i = 0; i < indent; i++) {
            indentStr.append(" ");
        }
        std::vector<std::weak_ptr<fsreferrer>> refs;
        {
            std::lock_guard lock{*mtx};
            for (const rawfsreference &ref: this->refs) {
                refs.push_back(ref.referrer);
            }
        }
        for (auto &ref : refs) {
            std::shared_ptr<fsreferrer> referrer = ref.lock();
            if (referrer) {
                std::cout << indentStr << referrer->GetType() << "(" << referrer->GetReferrerIdentifier() << ")\n";
                std::shared_ptr<fsresourceunwinder> unwinder = std::dynamic_pointer_cast<fsresourceunwinder>(referrer);
                if (unwinder) {
                    unwinder->PrintTraceBack(indent + 1);
                }
            } else {
                std::cout << indentStr << "*dead*\n";
            }
        }
    }
};

#endif //JEOKERNEL_FSRESOURCE_H
