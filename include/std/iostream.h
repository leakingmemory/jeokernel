//
// Created by sigsegv on 6/5/22.
//

#ifndef JEOKERNEL_IOSTREAM_H
#define JEOKERNEL_IOSTREAM_H

#include <std/std_string.h>
#include <std/ostream.h>

namespace std {
    class stdout_impl;

    class stdout : public std::basic_ostream<char, char_traits<char>> {
    private:
        stdout_impl *impl;
    public:
        stdout() noexcept;
        ~stdout();
        stdout& write(const char* s, std::streamsize count) override;
    };

    class stderr : public std::basic_ostream<char, char_traits<char>> {
    public:
        stderr() noexcept {}
        stderr& write(const char* s, std::streamsize count) override;
    };

    extern stdout cout;
    extern stderr cerr;
}

#endif //JEOKERNEL_IOSTREAM_H
