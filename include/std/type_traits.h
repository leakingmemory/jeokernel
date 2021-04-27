//
// Created by sigsegv on 27.04.2021.
//

#ifndef JEOKERNEL_TYPE_TRAITS_H
#define JEOKERNEL_TYPE_TRAITS_H

namespace std {
    template<class T>
    struct remove_reference {
        typedef T type;
    };
    template<class T>
    struct remove_reference<T &> {
        typedef T type;
    };
    template<class T>
    struct remove_reference<T &&> {
        typedef T type;
    };

    template< class T >
    using remove_reference_t = typename remove_reference<T>::type;

    template<bool B, class T = void> struct enable_if {};

    template<class T> struct enable_if<true, T> { typedef T type; };

    template<bool B, class T, class F> struct conditional { typedef T type; };

    template<class T, class F> struct conditional<false, T, F> { typedef F type; };
}

#endif //JEOKERNEL_TYPE_TRAITS_H
