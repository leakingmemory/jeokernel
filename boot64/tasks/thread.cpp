//
// Created by sigsegv on 09.05.2021.
//

#include <thread>
#include <core/scheduler.h>
#include <core/cpu_mpfp.h>
#include <core/LocalApic.h>
#include <std/thread.h>


extern "C" {
    void thread_trampoline_start(std::threadimpl::thread_start_context *ctx);

    void thread_trampoline(std::threadimpl::thread_start_context *ctx) {
        ctx->invoke();
        uint8_t cpu_num{0};
        {
            cpu_mpfp *mpfp = get_mpfp();
            LocalApic lapic{*mpfp};
            cpu_num = lapic.get_cpu_num(*mpfp);
        }
        tasklist *scheduler = get_scheduler();
        scheduler->exit(cpu_num);
    }
}

namespace std {

    uint32_t thread::start() {
        tasklist *scheduler = get_scheduler();
        {
            std::vector<task_resource *> resources{};
            return scheduler->new_task((uint64_t) thread_trampoline_start, 0x8, (uint64_t) start_context, 0, 0, 0, 0, 0, resources);
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