//
// Created by sigsegv on 11/11/23.
//

#ifndef JEOKERNEL_RESOURCE_RESOURCE_H
#define JEOKERNEL_RESOURCE_RESOURCE_H

#include <memory>
#include <vector>
#include <mutex>
#include <string>
#include <iostream>
#include <concurrency/hw_spinlock.h>
#include <resource/referrer.h>

//#define RESOURCE_REF_TRACE
//#define RESOURCE_CHECK_MAGIC

struct rawreference {
    std::weak_ptr<class referrer> referrer;
    uint64_t id;
};

constexpr uint32_t resource_magic = 0x4589;

struct rawresource {
    hw_spinlock mtx{};
    std::vector<rawreference> refs{};
    uint64_t serial{0};
#ifdef RESOURCE_CHECK_MAGIC
    uint16_t magic{resource_magic};
#endif
    void Forget(uint64_t id);
    uint64_t NewRef(const std::shared_ptr<referrer> &);
};

class resourceunwinder {
public:
    virtual void PrintTraceBack(int indent) = 0;
};

template <class T> class reference;
template <class T> class reference_reference;

template <class T> class resource : private rawresource, public resourceunwinder {
    friend reference<T>;
    friend reference_reference<T>;
private:
    std::weak_ptr<resource<T>> self_ref{};
#ifdef RESOURCE_REF_TRACE
    std::vector<std::string> trace{};
#endif
protected:
    void SetSelfRef(const std::shared_ptr<resource<T>> &ref) {
        std::weak_ptr<resource<T>> w{ref};
        self_ref = w;
    }
    virtual T *GetResource() = 0;
public:
    resource() {
    }
    resource(const resource &) = delete;
    resource(resource &&) = delete;
    resource &operator =(const resource &) = delete;
    resource &operator =(resource &&) = delete;
    reference<T> CreateReference(const std::shared_ptr<referrer> &referrer) {
        if (!referrer) {
            asm("ud2");
        }
#ifdef RESOURCE_CHECK_MAGIC
        if (magic != resource_magic) {
            asm("ud2");
        }
#endif
        std::weak_ptr<class referrer> w{referrer};
        uint64_t id;
        {
            std::lock_guard lock{mtx};
            rawreference &ref = refs.emplace_back();
            ref.referrer = w;
            ref.id = ++serial;
            id = ref.id;
#ifdef RESOURCE_REF_TRACE
            trace.push_back(referrer->GetType());
#endif
        }
        reference<T> ref{};
        ref.ref = std::make_shared<reference_reference<T>>(self_ref.lock(), id);
        ref.ptr = (T *) ref.ref->GetResource();
        return ref;
    }

    void AddTrace(const std::string &msg) {
#ifdef RESOURCE_REF_TRACE
        std::lock_guard lock{mtx};
        trace.push_back(msg);
#endif
    }
    void PrintTraceBack(int indent) override {
        std::string indentStr{};
        for (int i = 0; i < indent; i++) {
            indentStr.append(" ");
        }
        std::vector<std::weak_ptr<referrer>> refs;
#ifdef RESOURCE_REF_TRACE
        std::vector<std::string> trace{};
#endif
        {
            std::lock_guard lock{mtx};
            for (const rawreference &ref: this->refs) {
                refs.push_back(ref.referrer);
            }
#ifdef RESOURCE_REF_TRACE
            if (refs.empty()) {
                for (const auto &t : this->trace) {
                    trace.push_back(t);
                }
            }
#endif
        }
#ifdef RESOURCE_REF_TRACE
        for (auto &t : trace) {
            std::cout << indentStr << t << " (trace)\n";
        }
#endif
        for (auto &ref : refs) {
            std::shared_ptr<referrer> referrer = ref.lock();
            if (referrer) {
                std::cout << indentStr << referrer->GetType() << "(" << referrer->GetReferrerIdentifier() << ")\n";
                std::shared_ptr<resourceunwinder> unwinder = std::dynamic_pointer_cast<resourceunwinder>(referrer);
                if (unwinder) {
                    unwinder->PrintTraceBack(indent + 1);
                }
            } else {
                std::cout << indentStr << "*dead*\n";
            }
        }
    }
};

#endif //JEOKERNEL_RESOURCE_RESOURCE_H
