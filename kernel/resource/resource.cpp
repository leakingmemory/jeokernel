//
// Created by sigsegv on 11/11/23.
//

#include <resource/resource.h>

void rawresource::Forget(uint64_t id) {
    std::lock_guard lock{mtx};
    auto iterator = refs.begin();
    while (iterator != refs.end()) {
        if (iterator->id == id) {
            refs.erase(iterator);
            return;
        }
        ++iterator;
    }
}

uint64_t rawresource::NewRef(const std::shared_ptr<referrer> &referrer) {
#ifdef RESOURCE_CHECK_MAGIC
    if (magic != resource_magic) {
        asm("ud2");
    }
#endif
    uint64_t id;
    std::weak_ptr<class referrer> w{referrer};
    {
        std::lock_guard lock{mtx};
        rawreference &ref = refs.emplace_back();
        ref.referrer = w;
        ref.id = ++serial;
        id = ref.id;
    }
    return id;
}
