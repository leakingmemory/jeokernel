//
// Created by sigsegv on 4/23/22.
//

#ifndef FSBITS_DIRECTORY_H
#define FSBITS_DIRECTORY_H

#include <string>
#include <vector>
#include <memory>
#include <files/fileitem.h>

class directory_entry {
private:
    std::string name;
    std::shared_ptr<fileitem> item;
public:
    directory_entry(const std::string &name, std::shared_ptr<fileitem> item) : name(name), item(item) {}
    std::string Name() {
        return name;
    }
    std::shared_ptr<fileitem> Item() {
        return item;
    }
};

struct entries_result {
    std::vector<std::shared_ptr<directory_entry>> entries;
    fileitem_status status;
};

struct directory_resolve_result {
    std::shared_ptr<fileitem> file;
    fileitem_status status;
};

class directory : public fileitem {
public:
    virtual ~directory() = default;
    uint32_t Mode() override = 0;
    virtual entries_result Entries() = 0;
    directory_resolve_result Resolve(std::string filename);
};

#endif //FSBITS_DIRECTORY_H
