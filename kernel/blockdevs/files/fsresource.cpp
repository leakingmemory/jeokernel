//
// Created by sigsegv on 11/5/23.
//

#include <files/fsresource.h>

void rawfsresource::Forget(uint64_t id) {
    std::lock_guard lock{*mtx};
    auto iterator = refs.begin();
    while (iterator != refs.end()) {
        if (iterator->id == id) {
            refs.erase(iterator);
            return;
        }
        ++iterator;
    }
}

uint64_t rawfsresource::NewRef(const std::shared_ptr<fsreferrer> &referrer) {
    uint64_t id;
    std::weak_ptr<fsreferrer> w{referrer};
    {
        std::lock_guard lock{*mtx};
        rawfsreference &ref = refs.emplace_back();
        ref.referrer = w;
        ref.id = ++serial;
        id = ref.id;
    }
    return id;
}