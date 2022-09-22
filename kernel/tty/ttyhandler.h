//
// Created by sigsegv on 9/22/22.
//

#ifndef JEOKERNEL_TTYHANDLER_H
#define JEOKERNEL_TTYHANDLER_H

#include <memory>
#include <string>

class ttyhandler {
private:
    ttyhandler();
public:
    static std::shared_ptr<ttyhandler> Create(const std::string &name);
};


#endif //JEOKERNEL_TTYHANDLER_H
