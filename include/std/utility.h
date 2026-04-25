//
// Created by sigsegv on 27.04.2021.
//

#ifndef JEOKERNEL_UTILITY_H
#define JEOKERNEL_UTILITY_H

#include <type_traits>

namespace std {
    template<class T>
    constexpr std::remove_reference_t<T> &&move(T &&t) noexcept {
        return static_cast<typename std::remove_reference<T>::type&&>(t);
    }
    template<class T> constexpr T&& forward(typename std::remove_reference<T>::type &&t) noexcept {
        return t;
    }
    template<class T> constexpr T&& forward(typename std::remove_reference<T>::type &t) noexcept {
        return static_cast<T&&>(t);
    }
}

#endif //JEOKERNEL_UTILITY_H
