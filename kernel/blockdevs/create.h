//
// Created by sigsegv on 9/13/23.
//

#ifndef FSBITS_CREATE_H
#define FSBITS_CREATE_H

#include <memory>
#include <string>
#include <vector>
#include <files/directory.h>

template <class T> class fsreference;

int create(const fsreference<directory> &rootdir, std::vector<std::string>::iterator &args, const std::vector<std::string>::iterator &args_end);

#endif //FSBITS_CREATE_H
