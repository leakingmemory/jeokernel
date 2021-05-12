//
// Created by sigsegv on 27.04.2021.
//

#ifndef JEOKERNEL_TYPE_TRAITS_H
#define JEOKERNEL_TYPE_TRAITS_H

namespace std {
    template <class T> struct type_identity {
        using type = T;
    };
}

#include <std/declval.h>

namespace std {
    template< class T, T v >
    struct integral_constant {
        static constexpr T value = v;
    };

    typedef integral_constant<bool, false> false_type;
    typedef integral_constant<bool, true> true_type;

    template <bool Value> using bool_constant = integral_constant<bool, Value>;

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

    template <class T> struct remove_pointer {
        typedef T type;
    };
    template <class T> struct remove_pointer<T *> {
        typedef T type;
    };
    template <class T> struct remove_pointer<T const *> {
        typedef T type;
    };
    template <class T> struct remove_pointer<T volatile *> {
        typedef T type;
    };
    template <class T> struct remove_pointer<T const volatile *> {
        typedef T type;
    };

    template< class T >
    using remove_reference_t = typename remove_reference<T>::type;

    template <class T> struct remove_cv {
        typedef T type;
    };
    template <class T> struct remove_cv<const T> {
        typedef T type;
    };
    template <class T> struct remove_cv<volatile T> {
        typedef T type;
    };
    template <class T> struct remove_cv<const volatile T> {
        typedef T type;
    };

    template <class T> using remove_cv_t = typename remove_cv<T>::type;

    template <class T> struct is_pointer_helper : false_type {};
    template <class T> struct is_pointer_helper<T *> : true_type {};
    template <class T> struct is_pointer : is_pointer_helper<typename remove_cv<T>::type> {};

    template <class Boolish> struct negation : bool_constant<!bool(Boolish::value)> {};

    template<bool B, class T = void> struct enable_if {};

    template<class T> struct enable_if<true, T> { typedef T type; };

    template<bool B, class T, class F> struct conditional { typedef T type; };

    template<class T, class F> struct conditional<false, T, F> { typedef F type; };

    template <class...> struct common_type {};

    template <class T> struct common_type<T> : common_type<T, T> {};

    template <class T1, class T2> struct common_type<T1, T2> {
        using type = typename std::remove_reference<decltype(false ? declval<T1>() : declval<T2>())>::type;
    };
}

#endif //JEOKERNEL_TYPE_TRAITS_H
