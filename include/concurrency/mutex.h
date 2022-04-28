//
// Created by sigsegv on 5/13/21.
//

#ifndef JEOKERNEL_CONCURRENCY_MUTEX_H
#define JEOKERNEL_CONCURRENCY_MUTEX_H

#include <cstdint>

typedef struct {
    uint32_t _before_cacheline[16];
    uint32_t cacheline[16];
    uint32_t _after_cacheline[16];
} jeokernel_mutex_fields;

#define jeokernel_mutex_field_initvalues { \
._before_cacheline = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, \
.cacheline = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, \
,_after_cacheline = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} \
}

#ifdef __cplusplus

namespace std {
    class mutex {
    private:
        jeokernel_mutex_fields data;
    public:
        mutex();

        mutex(const mutex &) = delete;
        mutex(mutex &&) = delete;
        mutex &operator =(const mutex &cp) {
            data = cp.data;
            return *this;
        }
        mutex &operator =(mutex &&mv) {
            data = mv.data;
            return *this;
        }

    private:
        uint32_t create_ticket() noexcept;
        bool try_acquire_ticket(uint32_t ticket) noexcept;
        uint32_t release_ticket() noexcept;
        uint32_t current_ticket() noexcept;
    public:
        bool try_lock() noexcept;
        void lock();
        void unlock() noexcept;
    };

    static_assert(sizeof(jeokernel_mutex_fields) == sizeof(mutex));
}

#endif

#endif //JEOKERNEL_CONCURRENCY_MUTEX_H
