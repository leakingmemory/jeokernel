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

    template <class T> struct remove_const {
        typedef T type;
    };
    template <class T> struct remove_const<const T> {
        typedef T type;
    };

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

    namespace impl {
        template<typename T>
        struct assignable_struct {
            T value;
        };

        template<typename T>
        auto assignable_f() noexcept -> assignable_struct<T> &;

        template<typename T>
        auto polymorphic_f() noexcept -> decltype(dynamic_cast<const void *>(static_cast<T *>(nullptr)));
    }

    template <typename T, typename V, typename = void> struct is_assignable {
        static constexpr bool value = false;
    };

    template <typename T, typename V> struct is_assignable<T, V, decltype(impl::assignable_f<T>().value = declval<V>(), void())> {
        static constexpr bool value = true;
    };

    static_assert(is_assignable<int,int>::value);
    static_assert(is_assignable<long,int>::value);
    static_assert(is_assignable<void *,int *>::value);
    static_assert(!is_assignable<char *,int *>::value);

    template <typename T, typename = void> struct is_polymorphic {
        static constexpr bool value = false;
    };
    template <typename T> struct is_polymorphic<T, decltype(impl::polymorphic_f<T>(), void())> {
        static constexpr bool value = true;
    };
}

#endif //JEOKERNEL_TYPE_TRAITS_H
