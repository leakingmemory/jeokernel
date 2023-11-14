//
// Created by sigsegv on 11/9/23.
//

#ifndef JEOKERNEL_KFILE_IMPL_H
#define JEOKERNEL_KFILE_IMPL_H

#include <kfs/kfiles.h>
#include <files/fsreference.h>
#include <files/fsreferrer.h>
#include <files/fsresource.h>
#include <resource/resource.h>

class kfile_impl;

template <class T> concept kfile_impl_sub = requires(T *a, kfile_impl *b) {
    {b = a};
};

class kfile_impl : public kfile, public referrer, public fsreferrer, public resource<kfile_impl>, public fsresourceunwinder {
    friend kdirectory_impl;
private:
    fsreference<fileitem> file{};
protected:
    kfile_impl(const std::string &name) : kfile(name), referrer("kfile_impl"), fsreferrer("kfile_impl"), resource<kfile_impl>() {}
    kfile_impl *GetResource() override;
public:
    kfile_impl() = delete;
    kfile_impl(const kfile_impl &) = delete;
    kfile_impl(kfile_impl &&) = delete;
    kfile_impl &operator =(const kfile_impl &) = delete;
    kfile_impl &operator =(kfile_impl &&) = delete;
    virtual ~kfile_impl() = default;
    void Init(const std::shared_ptr<kfile_impl> &selfRef, const fsreference<fileitem> &resource);
    template <kfile_impl_sub T, typename... Args> static std::shared_ptr<T> Create(const fsreference<fileitem> &file, Args... args) {
        std::shared_ptr<T> shobj{new T(args...)};
        shobj->kfile_impl::Init(shobj, file);
        return shobj;
    }
    std::string GetReferrerIdentifier() override;
    void PrintTraceBack(int indent) override;
    fileitem *GetFileitem() const override;
    uint32_t Mode() const override;
    std::size_t Size() const override;
    uintptr_t InodeNum() const override;
    uint32_t BlockSize() const override;
    uintptr_t SysDevId() const override;
    kfile_result<std::size_t> Read(uint64_t offset, void *ptr, std::size_t len) override;
    kfile_result<filepage_ref> GetPage(std::size_t pagenum) override;
    fsreference<fileitem> GetImplementation(const std::shared_ptr<fsreferrer> &) const;
    fileitem &GetImplementation() const;
};


#endif //JEOKERNEL_KFILE_IMPL_H
