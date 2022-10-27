//
// Created by sigsegv on 9/15/21.
//

#include "tests.h"

class critical_section {
public:
    critical_section() = default;
};

#define WEAK_PTR_CRITICAL_SECTION critical_section

#include "../include/std/memory.h"

class dirparent {
protected:
    bool &flag;
public:
    explicit dirparent(bool &flag) : flag(flag) {}
    virtual ~dirparent() = default;
};

class otherparent {
private:
    int sig;
public:
    otherparent() : sig(1234) {}
    [[nodiscard]] int Sig() const {return sig;}
};

class destruction : public dirparent, public otherparent {
private:
    int magic_v;
public:
    explicit destruction(bool &flag) : dirparent(flag), magic_v(1337) {
        assert(!flag);
    }
    ~destruction() override {
        assert(!flag);
        flag = true;
    }
    [[nodiscard]] int magic() const {
        return magic_v;
    }
};

class grandch : public destruction {
public:
    explicit grandch(bool &flag) : destruction(flag) {}
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
        std::shared_ptr<destruction> shared = create_shared(flag);
        std::shared_ptr<otherparent> other{shared};
        assert(other);
        assert(other->Sig() == 1234);
        std::shared_ptr<grandch> gr{shared};
        assert(!gr);
    }
    {
        bool flag{false};
        std::shared_ptr<destruction> shared = create_shared(flag);
        std::shared_ptr<otherparent> other{};
        assert(!other);
        other = shared;
        assert(other);
        assert(other->Sig() == 1234);
        std::shared_ptr<grandch> gr{shared};
        assert(!gr);
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
        std::shared_ptr<destruction> *ptr;
        std::shared_ptr<destruction> *ptr2;
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