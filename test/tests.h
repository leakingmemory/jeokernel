//
// Created by sigsegv on 23.04.2021.
//

#ifndef JEOKERNEL_TESTS_H
#define JEOKERNEL_TESTS_H

#include <iostream>

void assertion_failed(const char *condition, const char *filename, int lineno) {
    std::cerr << "Assertion " << condition << " failed at " << filename << ":" << lineno << std::endl;
    exit(1);
}

#define assert(condition) { if (!(condition)) { assertion_failed(#condition, __FILE__, __LINE__); } }

#endif //JEOKERNEL_TESTS_H
