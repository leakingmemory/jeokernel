//
// Created by sigsegv on 7/1/21.
//

#ifndef JEOKERNEL_SIGNAL_H
#define JEOKERNEL_SIGNAL_H

#include <cstdint>
#include <errno.h>

#define SIGNALSET_MAX_V (64)
#ifdef __cplusplus
constexpr int SignalMax() { return SIGNALSET_MAX_V; }
#define SIGNALSET_MAX (SignalMax())
#else
#define SIGNALSET_MAX SIGNALSET_MAX_V
#endif

#ifdef __cplusplus

#include <type_traits>

#endif

struct __sigset_t {
    typedef uint32_t sigset_val_t;
    sigset_val_t val[SIGNALSET_MAX / (8 * sizeof(sigset_val_t))];

#ifdef __cplusplus

    constexpr __sigset_t() : val() {}

    constexpr __sigset_t operator ~() const {
        __sigset_t res;
        auto numPerItem = 8 * sizeof(sigset_val_t);
        for (int i = 0; i < ((SignalMax() + numPerItem - 1) / numPerItem); i++) {
            res.val[i] = ~val[i];
        }
        return res;
    }

    constexpr __sigset_t operator & (const __sigset_t &other) const {
        __sigset_t res;
        auto numPerItem = 8 * sizeof(sigset_val_t);
        for (int i = 0; i < ((SignalMax() + numPerItem - 1) / numPerItem); i++) {
            res.val[i] = val[i] & other.val[i];
        }
        return res;
    }

    constexpr int Set(int signal, bool value = true) {
        if (signal < 1 || signal > SignalMax()) {
            return -EINVAL;
        }
        --signal;
        auto bit = signal & ((sizeof(sigset_val_t) * 8) - 1);
        sigset_val_t &ref = val[signal / (sizeof(sigset_val_t) * 8)];
        sigset_val_t check = 1;
        check = check << bit;
        sigset_val_t mask = ~check;
        check = value ? check : 0;
        ref = (ref & mask) | check;
        return 0;
    }

    constexpr void Clear(int signal) {
        Set(signal, false);
    }

    constexpr bool Test(int signal) {
        if (signal < 1 || signal > SignalMax()) {
            return false;
        }
        --signal;
        auto bit = signal & ((sizeof(sigset_val_t) * 8) - 1);
        sigset_val_t &ref = val[signal / (sizeof(sigset_val_t) * 8)];
        sigset_val_t check = 1;
        check = check << bit;
        return (ref & check) != 0;
    }

    constexpr int First() const {
        auto numPerItem = 8 * sizeof(sigset_val_t);
        for (int i = 0; i < ((SignalMax() + numPerItem - 1) / numPerItem); i++) {
            sigset_val_t value = val[i];
            if (value != 0) {
                int bit = 0;
                while ((value & 1) == 0) {
                    value = value >> 1;
                    ++bit;
                }
                return (i * (8 * (int)sizeof(sigset_val_t))) + bit + 1;
            }
        }
        return -1;
    }

#endif
};

typedef __sigset_t sigset_t;

#ifdef __cplusplus

namespace SignalTests {
    template <typename = void> concept IsTestable = true;
    template <typename T> concept TestableItem = requires (T item) {
        { item.Check() ? true : false } -> IsTestable;
    };

    template <TestableItem ti> constexpr bool PerformTest() {
        ti item{};
        return item.Check();
    };

    struct Test0 {
        sigset_t sigs{};

        constexpr Test0() {}

        constexpr bool Check() {
            return !sigs.Test(1) && !sigs.Test(2) && sigs.val[0] == 0 && sigs.First() == -1;
        }
    };
    struct Test1 {
        sigset_t sigs{};

        constexpr Test1() {}

        constexpr bool Check() {
            sigs.Set(1);
            return sigs.Test(1) && !sigs.Test(2) && sigs.val[0] == 1 && sigs.First() == 1;
        }
    };
    struct Test2 {
        sigset_t sigs{};

        constexpr Test2() {}

        constexpr bool Check() {
            sigs.Set(11);
            return !sigs.Test(1) && !sigs.Test(2) && sigs.val[0] == (1 << 10) && sigs.First() == 11;
        }
    };
    struct Test3 {
        sigset_t sigs{};

        constexpr Test3() {}

        constexpr bool Check() {
            sigs.Set(34);
            return !sigs.Test(1) && sigs.Test(34) && sigs.val[1] == 2 && sigs.First() == 34;
        }
    };
    struct Test4 {
        sigset_t sigs{};

        constexpr Test4() {}

        constexpr bool Check() {
            sigs.Set(34);
            sigs.Clear(34);
            return !sigs.Test(1) && !sigs.Test(34) && sigs.val[1] == 0 && sigs.First() == -1;
        }
    };
    struct Test5 {
        sigset_t sigs{};

        constexpr Test5() {}

        constexpr bool Check() {
            sigs.Set(34);
            sigs.Set(35);
            sigs.Clear(34);
            return !sigs.Test(1) && !sigs.Test(34) && sigs.val[1] == 4 && sigs.First() == 35;
        }
    };

    static_assert(PerformTest<Test0>());
    static_assert(PerformTest<Test1>());
    static_assert(PerformTest<Test2>());
    static_assert(PerformTest<Test3>());
    static_assert(PerformTest<Test4>());
    static_assert(PerformTest<Test5>());
}
extern "C" {
#endif

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

typedef void (*sighandler_t)(int);

#define SIG_IGN ((sighandler_t) (void *) 0)

sighandler_t signal(int signal, sighandler_t handler);

#define SIGINT 2

typedef struct {
    int si_signo;
    int si_errno;
    int si_code;
    int pad;
    char data_area[128];
} siginfo_t;

struct sigaction {
    union {
        sighandler_t sa_handler;
        void (*sa_sigaction)(int, siginfo_t *, void *);
    };
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)();
};

#ifdef __cplusplus
};
#endif

#endif //JEOKERNEL_SIGNAL_H
