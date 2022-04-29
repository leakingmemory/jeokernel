//
// Created by sigsegv on 4/29/22.
//

#include <klogger.h>

typedef int ReasonCode;

typedef ReasonCode (*UnwindTraceFunc)(void *context, void *);

extern "C" {
    void _Unwind_Resume(void *obj) {
        wild_panic("_Unwind_resume(..)");
    }

    ReasonCode _Unwind_RaiseException(void *exception) {
        wild_panic("_Unwind_RaiseException(..)");
    }

    ReasonCode _Unwind_Resume_or_Rethrow(void *exception) {
        wild_panic("_Unwind_Resume_or_Rethrow(..)");
    }

    ReasonCode _Unwind_Backtrace(UnwindTraceFunc func, void *) {
        wild_panic("_Unwind_Backtrace(..)");
    }

    void *_Unwind_GetLanguageSpecificData(void *unwindContext) {
        wild_panic("_Unwind_GetLanguageSpecificData(..)");
    }

    void _Unwind_SetIP(void *unwindContext, uintptr_t val) {
        wild_panic("_Unwind_SetIP(..)");
    }

    uintptr_t _Unwind_GetIP(void *unwindContext) {
        wild_panic("_Unwind_GetIP(..)");
    }

    void _Unwind_SetGR(void *unwindContext, int index, uintptr_t val) {
        wild_panic("_Unwind_SetGR(..)");
    }

    void *_Unwind_GetRegionStart(void *unwindContext) {
        wild_panic("_Unwind_GetRegionStart(..)");
    }

    uintptr_t _Unwind_GetTextRelBase(void *unwindContext) {
        wild_panic("_Unwind_GetTextRelBase(..)");
    }

    uintptr_t _Unwind_GetDataRelBase(void *unwindContext) {
        wild_panic("_Unwind_GetDataRelBase(..)");
    }
}