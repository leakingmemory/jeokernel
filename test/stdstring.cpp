//
// Created by sigsegv on 11/3/22.
//

#include <cstdint>
#include <cstddef>
#include <string.h>
#include <memory>
#include <limits>
#include <type_traits>

class critical_section {
public:
    critical_section() = default;
};

#define WEAK_PTR_CRITICAL_SECTION critical_section


namespace teststd {
    namespace std {
        typedef ::std::size_t size_t;
        template <typename T, typename U> struct is_assignable : ::std::is_assignable<T,U> {
        };
        template <typename P> struct is_polymorphic : ::std::is_polymorphic<P> {
        };
        template<class T> class allocator : public ::std::allocator<T> {
        };
        template <typename T> class numeric_limits : public ::std::numeric_limits<T> {
        };
    }

#include "../include/std/std_string.h"

}
#include "tests.h"

int main() {
    {
        teststd::std::string t{"a"};
        assert(t == "a");
        assert(t != "b");
        t.erase(0, 1);
        assert(t == "");
    }
    {
        teststd::std::string t{"ab"};
        t.erase(0, 1);
        assert(t == "b");
    }
    {
        teststd::std::string t{"abc"};
        t.erase(1, 1);
        assert(t == "ac");
    }
    {
        teststd::std::string t{"abcdefghijklmnopqrstuvqxyzABCDEFGHIJKLMNOPQRSTUVZ"};
        t.erase(0, 8);
        assert(t == "ijklmnopqrstuvqxyzABCDEFGHIJKLMNOPQRSTUVZ");
    }
    {
        teststd::std::string t{"abcdefghijklmnopqrstuvqxyzABCDEFGHIJKLMNOPQRSTUVZ"};
        t.erase(8, 8);
        assert(t == "abcdefghqrstuvqxyzABCDEFGHIJKLMNOPQRSTUVZ");
    }
    return 0;
}
