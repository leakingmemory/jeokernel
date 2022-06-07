//
// Created by sigsegv on 6/6/22.
//

#ifndef JEOKERNEL_KFILES_H
#define JEOKERNEL_KFILES_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class fileitem;
class directory;
class kdirectory;
class kdirectory_impl;

class kfile {
    friend kdirectory;
    friend kdirectory_impl;
private:
    std::shared_ptr<fileitem> file;
public:
    kfile(const std::shared_ptr<fileitem> &file) : file(file) {}
    virtual ~kfile() = default;
    uint32_t Mode();
    std::size_t Size();
    std::size_t Read(uint64_t offset, void *ptr, std::size_t len);
};

class kdirent {
private:
    std::string name;
    std::shared_ptr<kfile> file;
public:
    kdirent(const std::string &name, const std::shared_ptr<kfile> &file) : name(name), file(file) {}
    std::string Name() {
        return name;
    }
    std::shared_ptr<kfile> File() {
        return file;
    }
};

class kdirectory_impl : public kfile {
private:
    std::shared_ptr<kfile> parent;
    std::string kpath;
public:
    kdirectory_impl(const std::shared_ptr<kfile> &parent, const std::string &kpath, const std::shared_ptr<fileitem> &fileitem) : kfile(fileitem), parent(parent), kpath(kpath) {}
    std::vector<std::shared_ptr<kdirent>> Entries(std::shared_ptr<kfile> this_impl);
    void Mount(const std::shared_ptr<directory> &fsroot);
    std::string Kpath();
};

class kdirectory : public kfile {
private:
    std::shared_ptr<kfile> impl;
public:
    kdirectory(const std::shared_ptr<kfile> &impl) : kfile(impl->file), impl(impl) {}
    std::vector<std::shared_ptr<kdirent>> Entries();
    std::shared_ptr<kfile> Resolve(std::string filename);
    void Mount(const std::shared_ptr<directory> &fsroot);
    std::string Kpath();
};

std::shared_ptr<kdirectory> get_kernel_rootdir();
bool mount(std::shared_ptr<kfile> mountpoint, std::shared_ptr<directory> fsroot);

#endif //JEOKERNEL_KFILES_H
