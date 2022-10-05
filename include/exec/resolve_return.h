//
// Created by sigsegv on 10/4/22.
//

#ifndef JEOKERNEL_RESOLVE_RETURN_H
#define JEOKERNEL_RESOLVE_RETURN_H

#include <cstdint>

struct resolve_return_value {
private:
    resolve_return_value(bool async) : result(0), async(async), hasValue(false) {}
    resolve_return_value(intptr_t result) : result(result), async(false), hasValue(true) {}
public:
    intptr_t result;
    bool async;
    bool hasValue;

    static resolve_return_value AsyncReturn() {
        return resolve_return_value(true);
    }
    static resolve_return_value Return(intptr_t result) {
        return resolve_return_value(result);
    }
    static resolve_return_value NoReturn() {
        return resolve_return_value(false);
    }

};


#endif //JEOKERNEL_RESOLVE_RETURN_H
