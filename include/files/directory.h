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

class directory : public fileitem {
public:
    virtual ~directory() = default;
    uint32_t Mode() override = 0;
    virtual std::vector<std::shared_ptr<directory_entry>> Entries() = 0;
};

#endif //FSBITS_DIRECTORY_H
