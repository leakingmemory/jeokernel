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
#include <signal.h>

class tasklist;
class task;
class ProcThread;

class callctx_async {
public:
    static void HandleSignalInWhenNotRunning(tasklist *scheduler, task *t, ProcThread *procthread, struct sigaction sigaction);
    virtual void async() = 0;
    virtual void returnAsync(intptr_t value) = 0;
};


class callctx_impl;
class ProcThread;

class callctx {
private:
    std::shared_ptr<callctx_impl> impl;
public:
    callctx() : impl() {}
    callctx(std::shared_ptr<callctx_async> async);
    ProcThread &GetProcess() const;
    void Aborter(const std::function<void ()> &);
    resolve_return_value Async() const;
    intptr_t Await(resolve_return_value returnValue) const;
    intptr_t Read(intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>) const;
    intptr_t Write(intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>) const;
    intptr_t ReadString(intptr_t ptr, std::function<resolve_return_value (const std::string &)>) const;
    resolve_return_value NestedRead(intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>) const;
    resolve_return_value NestedWrite(intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>) const;
    resolve_return_value NestedWrite(intptr_t ptr, intptr_t len, std::function<resolve_return_value (void *)>, std::function<resolve_return_value ()> fault) const;
    resolve_return_value NestedReadNullterminatedArrayOfPointers(intptr_t ptr, std::function<resolve_return_value (void **, size_t)>);
    resolve_return_value NestedReadString(intptr_t ptr, std::function<resolve_return_value (const std::string &)>) const;
    resolve_return_value NestedReadArrayOfStrings(void **vptr, size_t len, std::function<resolve_return_value (const std::vector<std::string> &)>);
    resolve_return_value Return(intptr_t result) const {
        return resolve_return_value::Return(result);
    }
    void ReturnWhenNotRunning(intptr_t value) const;
    void KillAsync() const;
    void EntrypointAsync(uintptr_t entrypoint, uintptr_t fsBase, uintptr_t stackPtr) const;
    std::shared_ptr<callctx_async> AsyncCtx() const;

    resolve_return_value ReturnFilter(std::function<resolve_return_value (resolve_return_value rv)>, std::function<resolve_return_value (std::shared_ptr<callctx>)> cll);
};

#endif //JEOKERNEL_CALLCTX_H
