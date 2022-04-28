//
// Created by sigsegv on 5/15/21.
//

#ifndef JEOKERNEL_RAW_SEMAPHORE_H
#define JEOKERNEL_RAW_SEMAPHORE_H

#include <cstdint>
typedef struct {
    uint32_t _before_cacheline[16];
    uint32_t cacheline[16];
    uint32_t _after_cacheline[16];
} jeokernel_raw_semaphore_fields;

#define jeokernel_raw_semaphore_field_initvalues { \
._before_cacheline = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, \
.cacheline = {0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, \
,_after_cacheline = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} \
}

#ifdef __cplusplus

class waiting_mul_semaphore;

class raw_semaphore {
    friend waiting_mul_semaphore;
private:
    jeokernel_raw_semaphore_fields data;
public:
    raw_semaphore(int32_t count);

    raw_semaphore(const raw_semaphore &) = delete;
    raw_semaphore(raw_semaphore &&) = delete;
    raw_semaphore &operator =(const raw_semaphore &cp) {
        data = cp.data;
        return *this;
    }
    raw_semaphore &operator =(raw_semaphore &&mv) {
        data = mv.data;
        return *this;
    }

private:
    uint32_t create_ticket() noexcept;
    bool try_acquire_ticket(uint32_t ticket) noexcept;
    uint32_t release_ticket() noexcept;
    uint32_t current_ticket() noexcept;
    uint32_t current_limit() noexcept;

public:
    bool try_acquire();
    void release();
private:
    void release_from_event();
public:
    void acquire();
};

static_assert(sizeof(jeokernel_raw_semaphore_fields) == sizeof(raw_semaphore));

#endif

#endif //JEOKERNEL_RAW_SEMAPHORE_H
