//
// Created by sigsegv on 9/15/21.
//

#include "tests.h"
#include "../include/std/memory.h"

class destruction {
private:
    bool &flag;
public:
    destruction(bool &flag) : flag(flag) {
        assert(!flag);
    }
    ~destruction() {
        assert(!flag);
        flag = true;
    }
    int magic() {
        return 1337;
    }
};

std::shared_ptr<destruction> create_shared(bool &flag) {
    return std::shared_ptr<destruction>(new destruction(flag));
}

int main() {
    {
        bool flag{false};
        {
            std::shared_ptr<destruction> shared = create_shared(flag);
            assert(shared);
            assert(shared->magic() == 1337);
            assert(!flag);
        }
        assert(flag);
    }
    {
        bool flag{false};
        {
            std::shared_ptr<destruction> cp{};
            assert(!cp);
            {
                std::shared_ptr<destruction> shared = create_shared(flag);
                assert(!flag);
                cp = shared;
                assert(cp);
                assert(!flag);
            }
            assert(cp->magic() == 1337);
            assert(!flag);
        }
        assert(flag);
    }
    {
        bool flag{false};
        std::shared_ptr<destruction> *ptr{nullptr};
        std::shared_ptr<destruction> *ptr2{nullptr};
        ptr = (std::shared_ptr<destruction> *) malloc(sizeof(*ptr));
        ptr2 = (std::shared_ptr<destruction> *) malloc(sizeof(*ptr2));
        {
            std::shared_ptr<destruction> shp = create_shared(flag);
            assert(!flag);
            new (ptr) std::shared_ptr<destruction>(shp);
        }
        assert(!flag);
        new (ptr2) std::shared_ptr<destruction>();
        *ptr2 = std::move(*ptr);
        assert(!flag);
        ptr->~shared_ptr<destruction>();
        assert(!flag);
        ptr2->~shared_ptr<destruction>();
        assert(flag);
        free(ptr2);
        free(ptr);
    }
}