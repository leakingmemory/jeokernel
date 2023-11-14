//
// Created by sigsegv on 11/11/23.
//

#ifndef JEOKERNEL_REFERRER_H
#define JEOKERNEL_REFERRER_H

#include <string>

class referrer {
private:
    std::string objtype;
public:
    referrer(const std::string &objtype) : objtype(objtype) {}
    virtual std::string GetReferrerIdentifier() = 0;
    [[nodiscard]] std::string GetType() const {
        return objtype;
    }
};

#endif //JEOKERNEL_REFERRER_H
