//
// Created by sigsegv on 10/18/23.
//

#ifndef FSBITS_MKDIR_H
#define FSBITS_MKDIR_H

#include <memory>
#include <string>
#include <vector>
#include <files/directory.h>

std::shared_ptr<directory> mkdir(std::shared_ptr<directory> rootdir, const std::string &ipath, bool pathCreate);
int mkdir(std::shared_ptr<directory> rootdir, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end);

#endif //FSBITS_MKDIR_H
