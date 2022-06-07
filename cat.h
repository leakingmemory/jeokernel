//
// Created by sigsegv on 6/7/22.
//

#ifndef FSBITS_CAT_H
#define FSBITS_CAT_H

#include <memory>
#include <string>
#include <vector>
#include <files/directory.h>

int cat(std::shared_ptr<directory> rootdir, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end);

#endif //FSBITS_CAT_H
