//
// Created by sigsegv on 12/6/22.
//

#ifndef FSBITS_SYMLINK_H
#define FSBITS_SYMLINK_H

#include <files/fileitem.h>

class symlink : public fileitem {
public:
    [[nodiscard]] virtual std::string GetLink() = 0;
};

#endif //FSBITS_SYMLINK_H
