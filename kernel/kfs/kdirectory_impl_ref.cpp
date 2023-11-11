//
// Created by sigsegv on 11/10/23.
//

#include "kdirectory_impl_ref.h"

std::shared_ptr<kfile> kdirectory_impl_ref::Load() {
    return ref;
}
