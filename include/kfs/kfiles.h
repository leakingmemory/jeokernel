//
// Created by sigsegv on 6/6/22.
//

#ifndef JEOKERNEL_KFILES_H
#define JEOKERNEL_KFILES_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
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
    virtual std::shared_ptr<kfile> Load() = 0;
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
    std::shared_ptr<kfile> File() {
        return file->Load();
    }
};

class filesystem;

class kdirectory : public kfile {
private:
    std::shared_ptr<kfile> impl;
public:
    kdirectory(const std::shared_ptr<kfile> &impl, const std::string &name) : kfile(name), impl(impl) {}
    kfile_result<std::vector<std::shared_ptr<kdirent>>> Entries();
    kfile_result<std::shared_ptr<kfile>> Resolve(kdirectory *root, std::string filename, int resolveSymlinks = 20);
    std::shared_ptr<filesystem> Unmount();
    void Mount(const std::string &devname, const std::string &fstype, const std::string &mntopts, const std::shared_ptr<filesystem> &fs);
    std::string Kpath();
    fileitem *GetFileitem() const override;
    uint32_t Mode() const override;
    std::size_t Size() const override;
    uintptr_t InodeNum() const override;
    uint32_t BlockSize() const override;
    uintptr_t SysDevId() const override;
    kfile_result<std::size_t> Read(uint64_t offset, void *ptr, std::size_t len) override;
    kfile_result<filepage_ref> GetPage(std::size_t pagenum) override;
};

class ksymlink : public kfile {
private:
    std::shared_ptr<ksymlink_impl> impl;
public:
    ksymlink(const std::shared_ptr<ksymlink_impl> &impl);
    [[nodiscard]] kfile_result<std::shared_ptr<kfile>> Resolve(kdirectory *root);
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
