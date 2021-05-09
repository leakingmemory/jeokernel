//
// Created by sigsegv on 09.05.2021.
//

#include <thread>
#include <core/scheduler.h>

extern "C" {
    void thread_trampoline(std::threadimpl::thread_start_context *ctx) {
        ctx->invoke();
    }
}

namespace std {

    void thread::start() {
        tasklist *scheduler = get_scheduler();
        {
            std::vector<task_resource *> resources{};
            scheduler->new_task((uint64_t) thread_trampoline, 0x8, (uint64_t) start_context, 0, 0, 0, 0, 0, resources);
        }
    }

}