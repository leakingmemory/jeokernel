//
// Created by sigsegv on 5/12/21.
//

#ifndef JEOKERNEL_STD_RATIO_H
#define JEOKERNEL_STD_RATIO_H

#include <std/numeric.h>
#include <cstdint>

namespace std {

    template <
    intmax_t Num,
    intmax_t Denom = 1
    > class ratio {
    public:
        constexpr static intmax_t num = (Num >= 0 ? 1 : -1) * (Denom >= 0 ? 1 : -1)
                * (Num >= 0 ? Num : (-Num)) / gcd(Num, Denom);
        constexpr static intmax_t den = (Denom >= 0 ? Denom : (-Denom)) / gcd(Num, Denom);
    };

    typedef ratio<1,1000000000000000000> atto;
    typedef ratio<1,1000000000000000> femto;
    typedef ratio<1,1000000000000> pico;
    typedef ratio<1,1000000000> nano;
    typedef ratio<1,1000000> micro;
    typedef ratio<1,1000> milli;
    typedef ratio<1,100> centi;
    typedef ratio<1,10> deci;
    typedef ratio<10,1> deca;
    typedef ratio<100,1> hecto;
    typedef ratio<1000,1> kilo;
    typedef ratio<1000000,1> mega;
    typedef ratio<1000000000,1> giga;
    typedef ratio<1000000000000,1> tera;
    typedef ratio<1000000000000000,1> peta;
    typedef ratio<1000000000000000000,1> exa;
}

#endif //JEOKERNEL_STD_RATIO_H
