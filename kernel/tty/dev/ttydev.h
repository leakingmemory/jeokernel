//
// Created by sigsegv on 9/22/22.
//

#ifndef JEOKERNEL_TTYDEV_H
#define JEOKERNEL_TTYDEV_H

#include <memory>
#include <string>

class ttyhandler;

class ttydev {
private:
    std::shared_ptr<ttyhandler> handler;
    ttydev(std::shared_ptr<ttyhandler> handler);
public:
    static std::shared_ptr<ttydev> Create(std::shared_ptr<ttyhandler> handler, const std::string &name);
};


#endif //JEOKERNEL_TTYDEV_H
