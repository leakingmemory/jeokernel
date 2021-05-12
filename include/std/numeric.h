//
// Created by sigsegv on 5/12/21.
//

#ifndef JEOKERNEL_NUMERIC_H
#define JEOKERNEL_NUMERIC_H

#include <std/type_traits.h>

namespace std {
    template <class A, class B> constexpr typename common_type<A, B>::type gcd(A a, B b) {
        if (a < 0) {
            a = -a;
        }
        if (b < 0) {
            b = -b;
        }
        if (a != 0) {
            if (b == 0) {
                return a;
            }

            /*
             * Hopefully this is a correct implementation of Euclidean algorithm
             */
            if (a > b) {
                return gcd(a % b, b);
            } else if (b > a) {
                return gcd(a, b % a);
            } else {
                return a;
            }
        } else {
            return b;
        }
    }

    static_assert(gcd(48,18) == 6);
    static_assert(gcd(18,48) == 6);
    static_assert(gcd(-18,48) == 6);
    static_assert(gcd(18,-48) == 6);
    static_assert(gcd(9,3) == 3);
    static_assert(gcd(6,2) == 2);
    static_assert(gcd(5,2) == 1);
    static_assert(gcd(4,2) == 2);
    static_assert(gcd(3,2) == 1);
    static_assert(gcd(2,2) == 2);
    static_assert(gcd(0,2) == 2);
    static_assert(gcd(0,-2) == 2);
}

#endif //JEOKERNEL_NUMERIC_H
