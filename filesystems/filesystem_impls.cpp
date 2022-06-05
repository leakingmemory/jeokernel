//
// Created by sigsegv on 5/19/22.
//

#include <filesystems/filesystem.h>
#include "ext2fs.h"

void register_filesystem_providers() {
    add_filesystem_provider(std::make_shared<ext2fs_provider>());
}
