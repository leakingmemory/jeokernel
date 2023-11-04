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
class kdirectory;
class kdirectory_impl;
class ksymlink_impl;
class ksymlink;

template <typename T> struct kfile_result {
    T result;
    kfile_status status;
};

class kfile {
    friend kdirectory;
    friend kdirectory_impl;
    friend ksymlink_impl;
    friend ksymlink;
private:
    std::shared_ptr<fileitem> file;
    std::string name;
public:
    kfile(const std::shared_ptr<fileitem> &file, const std::string &name) : file(file), name(name) {}
    virtual ~kfile() = default;
    std::string Name() const;
    uint32_t Mode() const;
    int Gid() const;
    int Uid() const;
    std::size_t Size() const;
    uintptr_t InodeNum() const;
    uint32_t BlockSize() const;
    uintptr_t SysDevId() const;
    kfile_result<std::size_t> Read(uint64_t offset, void *ptr, std::size_t len);
    kfile_result<filepage_ref> GetPage(std::size_t pagenum);
    std::shared_ptr<fileitem> GetImplementation() const;
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

class kdirectory_impl : public kfile {
private:
    std::shared_ptr<kfile> parent;
public:
    kdirectory_impl(const std::shared_ptr<kfile> &parent, const std::string &kpath, const std::shared_ptr<fileitem> &fileitem) : kfile(fileitem, kpath), parent(parent) {}
    kfile_result<std::vector<std::shared_ptr<kdirent>>> Entries(std::shared_ptr<kfile> this_impl);
    std::shared_ptr<filesystem> Unmount();
    void Mount(const std::string &devname, const std::string &fstype, const std::string &mntopts, const std::shared_ptr<filesystem> &fs, const std::shared_ptr<directory> &fsroot);
    std::string Kpath();
};

class kdirectory : public kfile {
private:
    std::shared_ptr<kfile> impl;
public:
    kdirectory(const std::shared_ptr<kfile> &impl, const std::string &name) : kfile(impl->file, name), impl(impl) {}
    kfile_result<std::vector<std::shared_ptr<kdirent>>> Entries();
    kfile_result<std::shared_ptr<kfile>> Resolve(kdirectory *root, std::string filename, int resolveSymlinks = 20);
    std::shared_ptr<filesystem> Unmount();
    void Mount(const std::string &devname, const std::string &fstype, const std::string &mntopts, const std::shared_ptr<filesystem> &fs, const std::shared_ptr<directory> &fsroot);
    std::string Kpath();
};

class ksymlink_impl : public kfile {
    friend ksymlink;
private:
    std::shared_ptr<kfile> parent;
public:
    ksymlink_impl(std::shared_ptr<kfile> parent, const std::shared_ptr<fileitem> &fileitem, const std::string &name) : kfile(fileitem, name), parent(parent) {}
};

class ksymlink : public kfile {
private:
    std::shared_ptr<ksymlink_impl> impl;
public:
    ksymlink(std::shared_ptr<ksymlink_impl> impl) : kfile(impl->file, impl->Name()), impl(impl) {}
    [[nodiscard]] kfile_result<std::shared_ptr<kfile>> Resolve(kdirectory *root);
    [[nodiscard]] std::string GetLink();
};

std::shared_ptr<kdirectory> get_kernel_rootdir();
bool mount(std::shared_ptr<kfile> mountpoint, std::shared_ptr<directory> fsroot);

#endif //JEOKERNEL_KFILES_H
