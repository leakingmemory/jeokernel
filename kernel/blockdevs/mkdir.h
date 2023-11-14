//
// Created by sigsegv on 10/18/23.
//

#ifndef FSBITS_MKDIR_H
#define FSBITS_MKDIR_H

#include <memory>
#include <string>
#include <vector>
#include <files/directory.h>

template <class T> class fsreference;

fsreference<directory> mkdir(const fsreference<directory> &rootdir, const std::string &ipath, bool pathCreate);
int mkdir(const fsreference<directory> &rootdir, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end);

#endif //FSBITS_MKDIR_H
