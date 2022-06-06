//
// Created by sigsegv on 6/5/22.
//

#include <iostream>
#include <concurrency/hw_spinlock.h>
#include <mutex>
#include <klogger.h>

namespace std {
    class stdout_impl {
    private:
        hw_spinlock mtx;
        basic_string<char> buffer;
    public:
        stdout_impl() : mtx(), buffer() {}
        void write(const char *s, std::streamsize count) {
            std::string printOut{};
            {
                std::lock_guard lock{mtx};
                buffer.append(s, count);
                bool found;
                for (char c : buffer) {
                    if (c == '\n') {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return;
                }
                printOut = buffer;
                buffer = "";
            }
            get_klogger() << printOut.c_str();
        }
    };

    stdout::stdout() noexcept : std::basic_ostream<char, char_traits<char>>(), impl(nullptr) {}

    stdout::~stdout() {}

    stdout &stdout::write(const char *s, std::streamsize count) {
        if (impl == nullptr) {
            impl = new stdout_impl();
        }
        impl->write(s, count);
        return *this;
    }

    stdout cout{};

    stderr &stderr::write(const char *s, std::streamsize count) {
        cout.write(s, count);
        return *this;
    }

    stderr cerr{};
}