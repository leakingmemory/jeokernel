//
// Created by sigsegv on 23.04.2021.
//

#ifndef JEOKERNEL_TESTS_H
#define JEOKERNEL_TESTS_H

void assertion_failed(const char *condition, const char *filename, int lineno);

#define assert(condition) { if (!(condition)) { assertion_failed(#condition, __FILE__, __LINE__); } }

#endif //JEOKERNEL_TESTS_H
