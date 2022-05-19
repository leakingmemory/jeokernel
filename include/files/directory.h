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
    std::string Name() {
        return name;
    }
    std::shared_ptr<fileitem> Item() {
        return item;
    }
};

class directory : public fileitem {
public:
    std::vector<std::shared_ptr<directory_entry>> Entries();
};

#endif //FSBITS_DIRECTORY_H
