//
// Created by sigsegv on 1/27/22.
//

#ifndef JEOKERNEL_KSHELL_H
#define JEOKERNEL_KSHELL_H

#include <thread>

class kshell {
private:
    std::mutex mtx;
    bool exit;
    std::thread shell;
public:
    kshell();
    ~kshell();
    void run();
};

#endif //JEOKERNEL_KSHELL_H
