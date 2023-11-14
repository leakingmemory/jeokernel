//
// Created by sigsegv on 11/10/23.
//

#include "kdirectory_impl_ref.h"
#include "kdirectory_impl.h"

kdirectory_impl_ref::kdirectory_impl_ref() : referrer("kdirectory_impl_ref"), ref() {
}

void kdirectory_impl_ref::Init(const std::shared_ptr<kdirectory_impl_ref> &selfRef,
                               const std::shared_ptr<kdirectory_impl> &ref) {
    this->ref = ref->CreateReference(selfRef);
}

void
kdirectory_impl_ref::Init(const std::shared_ptr<kdirectory_impl_ref> &selfRef, const reference<kdirectory_impl> &ref) {
    this->ref = ref->CreateReference(selfRef);
}

std::shared_ptr<kdirectory_impl_ref> kdirectory_impl_ref::Create(const std::shared_ptr<kdirectory_impl> &ref) {
    std::shared_ptr<kdirectory_impl_ref> impl_ref{new kdirectory_impl_ref()};
    impl_ref->Init(impl_ref, ref);
    return impl_ref;
}

std::shared_ptr<kdirectory_impl_ref> kdirectory_impl_ref::Create(const reference<kdirectory_impl> &ref) {
    std::shared_ptr<kdirectory_impl_ref> impl_ref{new kdirectory_impl_ref()};
    impl_ref->Init(impl_ref, ref);
    return impl_ref;
}

std::string kdirectory_impl_ref::GetReferrerIdentifier() {
    return "";
}

reference<kfile> kdirectory_impl_ref::Load(std::shared_ptr<class referrer> &referrer) {
    return ref->CreateReference(referrer);
}
