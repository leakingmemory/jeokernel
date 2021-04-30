//
// Created by sigsegv on 28.04.2021.
//

#ifndef JEOKERNEL_IOS_H
#define JEOKERNEL_IOS_H

#include <cstdint>

namespace std {

    typedef std::ssize_t streamsize;

    class ios_base {
    public:
        typedef int iostate;

        static constexpr iostate goodbit = 0;
        static constexpr iostate badbit = 4;
        static constexpr iostate failbit = 2;
        static constexpr iostate eofbit = 1;

        typedef uint32_t fmtflags;

        static constexpr fmtflags dec = 2;
        static constexpr fmtflags oct = 0;
        static constexpr fmtflags hex = 8;
        static constexpr fmtflags basefield = 63;
    private:
        iostate statebits;
        fmtflags fmt;
    public:
        ios_base() : statebits(goodbit), fmt(dec) {}

        iostate rdstate() const {
            return statebits;
        }

        void clear(std::ios_base::iostate state = std::ios_base::goodbit) {
            statebits = state;
        }

        void setstate(iostate state) {
            clear(statebits |= state);
        }

        fmtflags flags() const {
            return fmt;
        }

        fmtflags flags(fmtflags flags) {
            fmtflags old_fmt = fmt;
            fmt = flags;
            return old_fmt;
        }
    };

    template<
            class CharT,
            class Traits = std::char_traits<CharT>
    >
    class basic_ios : public std::ios_base {

    };

    ios_base &dec(ios_base &ios) {
        ios.clear((ios.rdstate() & ~ios_base::basefield) | ios_base::dec);
    }

    ios_base &oct(ios_base &ios) {
        ios.clear((ios.rdstate() & ~ios_base::basefield) | ios_base::oct);
    }

    ios_base &hex(ios_base &ios) {
        ios.clear((ios.rdstate() & ~ios_base::basefield) | ios_base::hex);
    }

}

#endif //JEOKERNEL_IOS_H
