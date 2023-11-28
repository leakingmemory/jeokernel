//
// Created by sigsegv on 6/6/22.
//

#ifndef JEOKERNEL_KFILES_H
#define JEOKERNEL_KFILES_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <resource/referrer.h>
#include <resource/reference.h>
#include <resource/resource.h>
#include "filepage_data.h"

struct kmount_info {
    std::string devname;
    std::string fstype;
    std::string mntopts;
    std::string name;
};

std::vector<kmount_info> GetKmounts();

enum class kfile_status {
    SUCCESS,
    IO_ERROR,
    TOO_MANY_LINKS,
    NO_AVAIL_INODES,
    NO_AVAIL_BLOCKS,
    EXISTS,
    NOT_DIRECTORY
};

std::string text(kfile_status status);

class fileitem;
class directory;
class kfile_impl;
class kdirectory;
class kdirectory_impl;
class ksymlink_impl;
class ksymlink;
template <class T> class fsreference;
template <class T> class fsresource;

template <typename T> struct kfile_result {
    T result;
    kfile_status status;
};

class kfile {
    friend kfile_impl;
    friend kdirectory;
    friend kdirectory_impl;
    friend ksymlink_impl;
    friend ksymlink;
private:
    std::string name;
public:
    kfile(const std::string &name) : name(name) {}
    virtual ~kfile() = default;
    std::string Name() const;
    virtual fileitem *GetFileitem() const = 0;
    virtual uint32_t Mode() const = 0;
    virtual int Gid() const;
    virtual int Uid() const;
    virtual std::size_t Size() const = 0;
    virtual uintptr_t InodeNum() const = 0;
    virtual uint32_t BlockSize() const = 0;
    virtual uintptr_t SysDevId() const = 0;
    virtual kfile_result<std::size_t> Read(uint64_t offset, void *ptr, std::size_t len) = 0;
    virtual kfile_result<filepage_ref> GetPage(std::size_t pagenum) = 0;
};

class kstatable {
public:
    virtual void stat(struct stat64 &st) const = 0;
    virtual void stat(struct statx &st) const = 0;
};

class lazy_kfile {
public:
    lazy_kfile() {}
    virtual reference<kfile> Load(std::shared_ptr<referrer> &referrer) = 0;
};

class kdirent {
private:
    std::string name;
    std::shared_ptr<lazy_kfile> file;
public:
    kdirent(const std::string &name, const std::shared_ptr<lazy_kfile> &file) : name(name), file(file) {}
    std::string Name() {
        return name;
    }
    reference<kfile> File(std::shared_ptr<referrer> &referrer) {
        return file->Load(referrer);
    }
};

class filesystem;

class kdirectory : public kfile, public referrer, public resource<kdirectory> {
private:
    std::weak_ptr<kdirectory> selfRef{};
    reference<kfile> impl;
private:
    kdirectory(const std::string &name) : kfile(name), referrer("kdirectory"), resource<kdirectory>(), impl() {}
public:
    void Init(const std::shared_ptr<kdirectory> &selfRef);
    void Init(const std::shared_ptr<kdirectory> &selfRef, const reference<kfile> &impl);
    void Init(const std::shared_ptr<kdirectory> &selfRef, const std::shared_ptr<kdirectory_impl> &impl);
    static std::shared_ptr<kdirectory> Create(const reference<kfile> &impl, const std::string &name);
    static std::shared_ptr<kdirectory> Create(const reference<kdirectory_impl> &impl, const std::string &name);
    static std::shared_ptr<kdirectory> Create(const std::shared_ptr<kdirectory_impl> &impl, const std::string &name);
    std::string GetReferrerIdentifier() override;
    kdirectory * GetResource() override;
    kfile_result<std::vector<std::shared_ptr<kdirent>>> Entries();
    kfile_result<reference<kfile>> Resolve(kdirectory *root, std::shared_ptr<class referrer> &referrer, std::string filename, int resolveSymlinks = 20);
    std::shared_ptr<filesystem> Unmount();
    void Mount(const std::string &devname, const std::string &fstype, const std::string &mntopts, const std::shared_ptr<filesystem> &fs);
    std::string Kpath();
    kfile_result<reference<kdirectory>> CreateDirectory(std::shared_ptr<class referrer> &referrer, std::string filename, uint16_t mode);
    fileitem *GetFileitem() const override;
    uint32_t Mode() const override;
    std::size_t Size() const override;
    uintptr_t InodeNum() const override;
    uint32_t BlockSize() const override;
    uintptr_t SysDevId() const override;
    kfile_result<std::size_t> Read(uint64_t offset, void *ptr, std::size_t len) override;
    kfile_result<filepage_ref> GetPage(std::size_t pagenum) override;
};

class ksymlink : public kfile, public referrer, public resource<ksymlink> {
private:
    std::weak_ptr<ksymlink> selfRef;
    reference<ksymlink_impl> impl;
private:
    ksymlink(const std::string &name);
public:
    ksymlink() = delete;
    static std::shared_ptr<ksymlink> Create(const reference<ksymlink_impl> &impl);
    void Init(const std::shared_ptr<ksymlink> &selfRef, const reference<ksymlink_impl> &impl);
    std::string GetReferrerIdentifier() override;
    ksymlink * GetResource() override;
    [[nodiscard]] kfile_result<reference<kfile>> Resolve(kdirectory *root, std::shared_ptr<class referrer> &referer);
    [[nodiscard]] std::string GetLink();
    fileitem *GetFileitem() const override;
    uint32_t Mode() const override;
    std::size_t Size() const override;
    uintptr_t InodeNum() const override;
    uint32_t BlockSize() const override;
    uintptr_t SysDevId() const override;
    kfile_result<std::size_t> Read(uint64_t offset, void *ptr, std::size_t len) override;
    kfile_result<filepage_ref> GetPage(std::size_t pagenum) override;
};

std::shared_ptr<kdirectory> get_kernel_rootdir();
bool mount(std::shared_ptr<kfile> mountpoint, std::shared_ptr<directory> fsroot);

#endif //JEOKERNEL_KFILES_H
