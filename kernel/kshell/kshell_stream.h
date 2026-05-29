//
// Created by sigsegv on 5/28/26.
//

#ifndef JEOKERNEL_KSHELL_STREAM_H
#define JEOKERNEL_KSHELL_STREAM_H

#include <std/std_string.h>
#include <std/ostream.h>

class kshell;

class kshell_stream : public std::basic_ostream<char, std::char_traits<char>> {
private:
    std::shared_ptr<kshell> sh;
public:
    kshell_stream(const std::shared_ptr<kshell> &sh) : sh(sh) {}
    kshell_stream(std::shared_ptr<kshell> &&sh) : sh(std::move(sh)) {}
    ~kshell_stream() = default;
    kshell_stream &write(const char* s, std::streamsize count) override;
};


#endif //JEOKERNEL_KSHELL_STREAM_H
