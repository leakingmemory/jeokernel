//
// Created by sigsegv on 9/22/22.
//

#ifndef JEOKERNEL_TTYFSDEV_H
#define JEOKERNEL_TTYFSDEV_H

#include <devfs/devfs.h>
#include "ttydev.h"
#include <kfs/kfiles.h>
#include <memory>

class ttyfsdev : public devfs_node, public kstatable {
private:
    std::shared_ptr<ttydev> ttyd;
public:
    ttyfsdev(std::shared_ptr<ttydev> ttyd) : devfs_node(00666), ttyd(ttyd) {}
    void stat(struct stat64 &st) const override;
    void stat(struct statx &st) const override;
};


#endif //JEOKERNEL_TTYFSDEV_H
