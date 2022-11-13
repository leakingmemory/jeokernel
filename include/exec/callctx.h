//
// Created by sigsegv on 10/4/22.
//

#ifndef JEOKERNEL_CALLCTX_H
#define JEOKERNEL_CALLCTX_H

#include <cstdint>
#include <functional>
#include <memory>
#include <exec/resolve_return.h>
#include <string>
#include <vector>

class callctx_async {
public:
    virtual void async() = 0;
    virtual void returnAsync(intptr_t value) = 0;
};


class callctx_impl;
class ProcThread;

class callctx {
private:
    std::shared_ptr<callctx_impl> impl;
public:
    callctx(std::shared_ptr<callctx_async> async);
    ProcThread &GetProcess() const;
    resolve_return_value Async() const;
    intptr_t Await(resolve_return_value returnValue) const;
    intptr_t Read(intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>) const;
    intptr_t Write(intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>) const;
    intptr_t ReadString(intptr_t ptr, std::function<resolve_return_value (const std::string &)>) const;
    resolve_return_value NestedRead(intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>) const;
    resolve_return_value NestedWrite(intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>) const;
    resolve_return_value NestedReadNullterminatedArrayOfPointers(intptr_t ptr, std::function<resolve_return_value (void **, size_t)>);
    resolve_return_value NestedReadArrayOfStrings(void **vptr, size_t len, std::function<resolve_return_value (const std::vector<std::string> &)>);
    resolve_return_value Return(intptr_t result) const {
        return resolve_return_value::Return(result);
    }
    void ReturnWhenNotRunning(intptr_t value) const;
    std::shared_ptr<callctx_async> AsyncCtx() const;
};

#endif //JEOKERNEL_CALLCTX_H
