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