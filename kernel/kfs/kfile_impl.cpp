//
// Created by sigsegv on 11/9/23.
//

#include "kfile_impl.h"
#include <files/fsreference.h>
#include <files/fsresource.h>
#include <files/fileitem.h>

kfile_impl *kfile_impl::GetResource() {
    return this;
}

void kfile_impl::Init(const std::shared_ptr<kfile_impl> &selfRef, const fsreference<fileitem> &resource) {
    SetSelfRef(selfRef);
    if (resource) {
        file = resource.CreateReference(selfRef);
    }
}

std::string kfile_impl::GetReferrerIdentifier() {
    std::string id{"kfile_impl:"};
    id.append(name);
    return id;
}

void kfile_impl::PrintTraceBack(int indent) {
    resource::PrintTraceBack(indent);
}

fileitem *kfile_impl::GetFileitem() const {
    return &(*file);
}

uint32_t kfile_impl::Mode() const {
    return file ? file->Mode() : 0040555;
}

std::size_t kfile_impl::Size() const {
    return file ? file->Size() : 0;
}

uintptr_t kfile_impl::InodeNum() const {
    return file ? file->InodeNum() : 0;
}

uint32_t kfile_impl::BlockSize() const {
    return file ? file->BlockSize() : 0;
}

uintptr_t kfile_impl::SysDevId() const {
    return file ? file->SysDevId() : 0;
}

kfile_result<std::size_t> kfile_impl::Read(uint64_t offset, void *ptr, std::size_t len) {
    if (!file) {
        return {.result = 0, .status = kfile_status::IO_ERROR};
    }
    auto result = file->Read(offset, ptr, len);
    if (result.size > 0 || result.status == fileitem_status::SUCCESS) {
        return {.result = result.size, .status = kfile_status::SUCCESS};
    } else {
        return {.result = 0, .status = kfile_status::IO_ERROR};
    }
}

kfile_result<filepage_ref> kfile_impl::GetPage(std::size_t pagenum) {
    if (!file) {
        return {.result = {}, .status = kfile_status::IO_ERROR};
    }
    auto pageResult = file->GetPage(pagenum);
    if (pageResult.page) {
        auto page = pageResult.page;
        auto data = page->Raw();
        filepage_ref ref{data};
        data->initDone();
        return {.result = ref, .status = kfile_status::SUCCESS};
    }
    return {.result = {}, .status = kfile_status::IO_ERROR};
}

fsreference<fileitem> kfile_impl::GetImplementation(const std::shared_ptr<fsreferrer> &referrer) const {
    return file.CreateReference(referrer);
}

fileitem &kfile_impl::GetImplementation() const {
    return *file;
}
