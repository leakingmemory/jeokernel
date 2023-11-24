//
// Created by sigsegv on 4/23/22.
//

#ifndef FSBITS_DIRECTORY_H
#define FSBITS_DIRECTORY_H

#include <string>
#include <vector>
#include <memory>
#include <files/fileitem.h>
#include <files/fsreference.h>

struct directory_entry_result {
    fsreference<fileitem> file;
    fileitem_status status;
};

class fsreferrer;

class directory_entry {
private:
    std::string name;
public:
    directory_entry(const std::string &name) : name(name) {}
    virtual ~directory_entry() = default;
    std::string Name() {
        return name;
    }
    virtual directory_entry_result LoadItem(const std::shared_ptr<fsreferrer> &referrer) = 0;
};

struct entries_result {
    std::vector<std::shared_ptr<directory_entry>> entries;
    fileitem_status status;
};

struct directory_resolve_result {
    fsreference<fileitem> file;
    fileitem_status status;
};

constexpr int maxLinkRecurse = 50;

class directory_path_resolver;

class directory : public fileitem {
    friend directory_path_resolver;
public:
    virtual ~directory() = default;
    uint32_t Mode() override = 0;
    virtual entries_result Entries() = 0;
    directory_resolve_result Resolve(const std::shared_ptr<fsreferrer> &referrer, std::string filename, directory *rootdir = nullptr, int followSymlink = maxLinkRecurse);
    virtual directory_resolve_result CreateFile(const std::shared_ptr<fsreferrer> &, std::string filename, uint16_t mode);
    virtual directory_resolve_result CreateDirectory(const std::shared_ptr<fsreferrer> &, std::string filename, uint16_t mode);
};

#endif //FSBITS_DIRECTORY_H
