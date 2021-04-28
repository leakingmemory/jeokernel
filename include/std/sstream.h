//
// Created by sigsegv on 28.04.2021.
//

#ifndef JEOKERNEL_SSTREAM_H
#define JEOKERNEL_SSTREAM_H

#include <std/std_string.h>
#include <std/istream.h>

namespace std {

    template<
            class CharT,
            class Traits = std::char_traits<CharT>,
            class Allocator = std::allocator<CharT>
    > class basic_stringstream : public basic_iostream<CharT,Traits> {
    private:
        basic_string<CharT, Traits, Allocator> _str;

    public:
        basic_stringstream() : _str() {
        }

        basic_stringstream& write(const CharT* s, std::streamsize count) override {
            _str.append(s, count);
            return *this;
        }

        std::basic_string<CharT,Traits,Allocator> str() const& {
            return _str;
        }
    };

    typedef basic_stringstream<char> stringstream;
}

#endif //JEOKERNEL_SSTREAM_H
