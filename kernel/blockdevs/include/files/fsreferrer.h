//
// Created by sigsegv on 11/5/23.
//

#ifndef JEOKERNEL_FSREFERRER_H
#define JEOKERNEL_FSREFERRER_H

#include <string>

class fsreferrer {
private:
    std::string objtype;
public:
    fsreferrer(const std::string &objtype) : objtype(objtype) {}
    virtual std::string GetReferrerIdentifier() = 0;
    [[nodiscard]] std::string GetType() const {
        return objtype;
    }
};

#endif //JEOKERNEL_FSREFERRER_H
