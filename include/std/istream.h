//
// Created by sigsegv on 28.04.2021.
//

#ifndef JEOKERNEL_ISTREAM_H
#define JEOKERNEL_ISTREAM_H

#include <std/ios.h>
#include <std/ostream.h>

namespace std {
    template<

            class CharT,
            class Traits = std::char_traits<CharT>
    >
    class basic_istream : virtual public std::basic_ios<CharT, Traits> {

    };

    template<

            class CharT,
            class Traits = std::char_traits<CharT>
    > class basic_iostream : public basic_istream<CharT,Traits>, public basic_ostream<CharT,Traits> {

    };
};

#endif //JEOKERNEL_ISTREAM_H
