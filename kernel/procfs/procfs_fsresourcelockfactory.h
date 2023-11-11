//
// Created by sigsegv on 11/8/23.
//

#ifndef JEOKERNEL_PROCFS_FSRESOURCELOCKFACTORY_H
#define JEOKERNEL_PROCFS_FSRESOURCELOCKFACTORY_H

#include <files/fsresource.h>

class procfs_fsresourcelockfactory : public fsresourcelockfactory {
public:
    std::shared_ptr<fsresourcelock> Create() override;
};


#endif //JEOKERNEL_PROCFS_FSRESOURCELOCKFACTORY_H
