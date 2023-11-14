//
// Created by sigsegv on 6/4/22.
//

#ifndef FSBITS_LS_H
#define FSBITS_LS_H

#include <memory>
#include <string>
#include <vector>
#include <files/directory.h>

template <class T> class fsreference;

int ls(const fsreference<directory> &rootdir, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end);

#endif //FSBITS_LS_H
