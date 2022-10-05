//
// Created by sigsegv on 10/4/22.
//

#ifndef JEOKERNEL_CALLCTX_H
#define JEOKERNEL_CALLCTX_H

#include <cstdint>
#include <functional>
#include <memory>
#include <exec/resolve_return.h>

class callctx_async {
public:
    virtual void async() = 0;
    virtual void returnAsync(intptr_t value) = 0;
};


class callctx_impl;

class callctx {
private:
    std::shared_ptr<callctx_impl> impl;
public:
    callctx(std::shared_ptr<callctx_async> async);
    intptr_t Read(intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    intptr_t Write(intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>);
    resolve_return_value Return(intptr_t result) {
        return resolve_return_value::Return(result);
    }
};

#endif //JEOKERNEL_CALLCTX_H
