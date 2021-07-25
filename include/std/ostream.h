//
// Created by sigsegv on 28.04.2021.
//

#ifndef JEOKERNEL_OSTREAM_H
#define JEOKERNEL_OSTREAM_H

#include <std/ios.h>

namespace std {
    template<
            class CharT,
            class Traits = std::char_traits<CharT>
    >
    class basic_ostream : virtual public std::basic_ios<CharT, Traits> {
    public:
        virtual basic_ostream& write(const CharT* s, std::streamsize count) = 0;

        basic_ostream& operator<<( signed int value ) {
            return *this << (signed long int) value;
        }
        basic_ostream& operator<<( unsigned int value ) {
            return *this << (unsigned long int) value;
        }
        basic_ostream& operator<<( signed long int value ) {
            return *this << (signed long long) value;
        }
        basic_ostream& operator<<( unsigned long int value ) {
            return *this << (unsigned long long) value;
        }
        basic_ostream& operator<<( signed long long value ) {
            if (value >= 0) {
                return *this << (unsigned long long) value;
            } else {
                this->write("-", 1);
                return *this << (unsigned long long) (0 - value);
            }
        }
        basic_ostream& operator<<( unsigned long long value ) {
            const char basestr[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/\\|§!\"#¤%";
            CharT str[(64/3)+2];
            str[0] = 0;
            int base = (this->flags() & ios_base::basefield) + 8;
            static_assert(sizeof(basestr) >= ios_base::basefield);
            streamsize length = 0;
            do {
                uint8_t digit = (uint8_t) (value % base);
                value /= base;
                for (streamsize i = length; i > 0; i--) {
                    str[i] = str[i - 1];
                }
                str[0] = base < 32 ? (digit < 10 ? '0' + digit : basestr[digit - 10]) : basestr[digit];
                length++;
            } while (value > 0);
            write(&(str[0]), length);
            return *this;
        }

        basic_ostream & operator << (std::ios_base& (*func)(std::ios_base&)) {
            func(*this);
            return *this;
        }
    };
}

template <class CharT, class Traits = std::char_traits<CharT>>
        std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os,
           const CharT *str) {
    return os.write(str, Traits::length(str));
}

template <class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
std::basic_ostream<CharT, Traits>&
operator<<(std::basic_ostream<CharT, Traits>& os,
           const std::basic_string<CharT, Traits, Allocator>& str) {
    return os.write(str.data(), str.size());
}


#endif //JEOKERNEL_OSTREAM_H
