//
// Created by sigsegv on 10/26/22.
//

#include "tests.h"
#include <cstdint>
#include <type_traits>
#include <new>
#include <utility>

class critical_section {
public:
    critical_section() = default;
};

#define WEAK_PTR_CRITICAL_SECTION critical_section

#include "../include/std/memory.h"

class testclass {
private:
    int *flag;
public:
    explicit testclass(int &flag) : flag(&flag) {
    }
    ~testclass() {
        ++(*flag);
    }
    testclass(const testclass &) = delete;
    testclass(testclass &&) = delete;
    testclass &operator =(const testclass &)= delete;
    testclass &operator =(testclass &&) = delete;
};

int main() {
    int first{0};
    {
        std::weak_ptr<testclass> weak1{};
        {
            std::shared_ptr<testclass> shared1 = std::make_shared<testclass>(first);
            {
                std::weak_ptr<testclass> weak{shared1};
                weak1 = weak;
            }
            std::shared_ptr<testclass> copy1 = weak1.lock();
            assert(copy1);
        }
        assert(first == 1);
        std::shared_ptr<testclass> cp = weak1.lock();
        assert(!cp);
    }
    int second{0};
    {
        std::weak_ptr<testclass> weak1{};
        {
            std::shared_ptr<testclass> shared1 = std::make_shared<testclass>(second);
            {
                std::weak_ptr<testclass> weak{shared1};
                weak1 = weak;
            }
            std::shared_ptr<testclass> copy1 = weak1.lock();
            assert(copy1);
            std::shared_ptr<testclass> copy2 = weak1.lock();
            assert(copy2);
            std::shared_ptr<testclass> copy3 = weak1.lock();
            assert(copy3);
        }
        assert(first == 1);
        std::shared_ptr<testclass> cp = weak1.lock();
        assert(!cp);
    }
    return 0;
}