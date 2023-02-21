//
// Created by sigsegv on 11/12/21.
//

#include "tests.h"
#define TEST_VECTOR_NS
#include "../include/std/vector.h"


class RefCounter {
public:
    int ref;
    RefCounter() : ref(1) {}
};

class RefPointer {
public:
    RefCounter *counter;
    RefPointer() : counter(new RefCounter()) { }
    ~RefPointer() {
        if (counter != nullptr) {
            counter->ref--;
            if (counter->ref == 0) {
                delete counter;
            }
            counter = nullptr;
        }
    }
    RefPointer(const RefPointer &cp) : counter(cp.counter) {
        if (counter != nullptr) {
            counter->ref++;
        }
    }
    RefPointer(RefPointer &&mv) : counter(mv.counter) {
        mv.counter = nullptr;
    }

    RefPointer &operator =(const RefPointer &cp) {
        RefCounter *prev = counter;
        counter = cp.counter;
        if (counter != nullptr) {
            counter->ref++;
        }
        if (prev != nullptr) {
            prev->ref--;
            if (prev->ref == 0) {
                delete prev;
            }
        }
        return *this;
    }
    RefPointer &operator =(RefPointer &&mv) {
        RefCounter *prev = counter;
        counter = mv.counter;
        if (prev != nullptr && this != &mv) {
            prev->ref--;
            if (prev->ref == 0) {
                delete prev;
            }
        }
        return *this;
    }
};

int main() {
    RefPointer one{};
    RefPointer two{};
    RefPointer three{};
    assert(one.counter->ref == 1);
    assert(two.counter->ref == 1);
    assert(three.counter->ref == 1);
    testvector::vector<RefPointer> vec{};
    vec.push_back(one);
    vec.push_back(two);
    vec.push_back(three);
    assert(one.counter->ref == 2);
    assert(two.counter->ref == 2);
    assert(three.counter->ref == 2);
    auto iter = vec.begin();
    vec.erase(iter);
    assert(one.counter->ref == 1);
    assert(two.counter->ref == 2);
    assert(three.counter->ref == 2);
}
