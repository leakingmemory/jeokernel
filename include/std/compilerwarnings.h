//
// Created by sigsegv on 3/14/22.
//

#ifndef JEOKERNEL_COMPILERWARNINGS_H
#define JEOKERNEL_COMPILERWARNINGS_H

#if defined(__GNUC__) || defined(__clang__)

#define COMPILER_WARNINGS_PRAGMA(X) _Pragma(#X)

#define COMPILER_WARNINGS_PUSH() _Pragma("GCC diagnostic push")
#define COMPILER_WARNINGS_POP() _Pragma("GCC diagnostic pop")

#define GNU_AND_CLANG_IGNORE_WARNING(warning) COMPILER_WARNINGS_PRAGMA(GCC diagnostic ignored #warning)

#else

#define COMPILER_WARNINGS_PUSH()
#define COMPILER_WARNINGS_POP()

#define GNU_AND_CLANG_IGNORE_WARNING(warning)

#endif

#endif //JEOKERNEL_COMPILERWARNINGS_H
