//
// Created by sigsegv on 09.05.2021.
//

#include <core/scheduler.h>
#include <core/cpu_mpfp.h>
#include <std/thread.h>
#if defined(__aarch64__)
#include <smp.h>
#else
#include <ApStartup.h>
#endif


extern "C" {
    void thread_trampoline_start(std::threadimpl::thread_start_context *ctx);

    void thread_trampoline(std::threadimpl::thread_start_context *ctx) {
        ctx->invoke();
        uint8_t cpu_num{0};
        tasklist *scheduler = get_scheduler();
        {
            critical_section cli{};
#if defined(__aarch64__)
            cpu_num = get_cpu_num();
#else
            if (scheduler->is_multicpu()) {
                ApStartup *apStartup = GetApStartup();
                cpu_num = apStartup->GetCpuNum();
            }
#endif
            scheduler->exit(cpu_num, true);
        }
        while (true) {
#if defined(__aarch64__)
            asm("wfe");
#else
            asm("hlt");
#endif
        }
    }
}

namespace std {

    uint32_t thread::start() {
        tasklist *scheduler = get_scheduler();
        {
            std::vector<task_resource *> resources{};
#if defined(__aarch64__)
            return scheduler->new_task((uint64_t) thread_trampoline_start, (uint64_t) start_context, 0, 0, 0, 0, 0, resources);
#else
            return scheduler->new_task((uint64_t) thread_trampoline_start, 0x8, (uint64_t) start_context, 0, 0, 0, 0, 0, resources);
#endif
        }
    }

    void thread::join() {
        if (start_context != nullptr && start_context->is_done()) {
            return;
        }
        get_scheduler()->join(id);
    }

    void thread::detach() {
        if (start_context != nullptr) {
            start_context->detach();
            start_context = nullptr;
        }
    }
}