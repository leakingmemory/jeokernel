//
// Created by sigsegv on 2/21/23.
//

#include <iostream>

void assertion_failed(const char *condition, const char *filename, int lineno) {
    std::cerr << "Assertion " << condition << " failed at " << filename << ":" << lineno << std::endl;
    exit(1);
}
