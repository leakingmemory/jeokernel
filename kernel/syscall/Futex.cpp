//
// Created by sigsegv on 8/8/22.
//

#include <exec/procthread.h>
#include <exec/usermem.h>
#include <iostream>
#include <errno.h>
#include "Futex.h"

#define FUTEX_DEBUG

int64_t Futex::Call(int64_t uptr_addr, int64_t futex_op, int64_t val, int64_t uptr_timeout_or_val2, SyscallAdditionalParams &params) {
    uintptr_t uptr_addr2 = params.Param5();
    int64_t val3 = params.Param6();

    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = current_task->get_resource<ProcThread>();
    auto op = futex_op & FUTEX_COMMAND_MASK;
    switch (op) {
        case FUTEX_WAKE:
            uintptr_t wakeNum = ((uintptr_t) val) & 0x7FFFFFFFULL;
            bool wakeAll = wakeNum == 0x7FFFFFFF;
            if (wakeAll) {
#ifdef FUTEX_DEBUG
                std::cout << "futex(" << std::hex << uptr_addr << "): wake all\n" << std::dec;
#endif
                return process->wake_all(uptr_addr);
            } else {
                std::cout << "futex(" << std::hex << uptr_addr << "): wake " << std::dec << wakeNum << "\n";
            }
            while (true) {
                asm("hlt");
            }
            break;
    }
    auto result = process->resolve_read(uptr_addr, sizeof(uint32_t), false, [scheduler, current_task] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = result;
            current_task->set_blocked(false);
        });
    }, [process, uptr_addr, futex_op, val, uptr_timeout_or_val2, uptr_addr2, val3, op] (bool success, bool, const std::function<void (intptr_t)>&) {
        if (!success) {
            return resolve_return_value::Return(-EFAULT);
        }
        switch (op) {
            case FUTEX_WAIT:
                uint32_t current_val;
                {
                    UserMemory mem{*process, (uintptr_t) uptr_addr, sizeof(uint32_t), true};
                    if (!mem) {
                        return resolve_return_value::Return(-EFAULT);
                    }
                    uintptr_t addr = (uintptr_t) mem.Pointer();
                    uint32_t current = val;
                    asm("mov %0, %%rcx; mov %1, %%rax; lock cmpxchgl %%eax, (%%rcx); mov %%rax, %1" :: "m"(addr), "m"(current) : "%rax", "%rcx");
                    if (val == current) {
                        std::cout << "Futex goes to sleep\n";
                        while (true) {
                            asm("hlt");
                        }
                    } else {
                        std::cout << "Futex returns due " <<std::dec<< val << "!=" << current << "\n";
                        while (true) {
                            asm("hlt");
                        }
                    }
                }
                break;
        }
        std::cout << "futex(" << std::hex << uptr_addr << ", " << futex_op << ", " << val << ", "
                  << uptr_timeout_or_val2 << ", " << uptr_addr2 << ", " << val3 << std::dec << ")\n";
        while (true) {
            asm("hlt");
        }
    });
    if (result.async) {
        params.DoContextSwitch(true);
        current_task->set_blocked(true);
        return 0;
    } else {
        return result.result;
    }
}
