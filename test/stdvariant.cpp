#include "tests.h"
#include "../include/std/variant.h"

struct VariantVisitCounts {
    int a_visit{0};
    int b_visit{0};
    int c_visit{0};
    int a_destroyed{0};
    int b_destroyed{0};
    int c_destroyed{0};
};

class VariantTypeA {
private:
    VariantVisitCounts &counts;
public:
    VariantTypeA(VariantVisitCounts &counts) : counts(counts) {}
    VariantTypeA(const VariantTypeA &) = delete;
    VariantTypeA(VariantTypeA &&mv) = delete;
    VariantTypeA &operator =(const VariantTypeA &) = delete;
    VariantTypeA &operator =(VariantTypeA &&) = delete;
    void visit() {
        ++counts.a_visit;
    }
    ~VariantTypeA() {
        ++counts.a_destroyed;
    }
};

class VariantTypeB {
private:
    VariantVisitCounts &counts;
public:
    VariantTypeB(VariantVisitCounts &counts) : counts(counts) {}
    VariantTypeB(const VariantTypeB &) = delete;
    VariantTypeB(VariantTypeB &&mv) = delete;
    VariantTypeB &operator =(const VariantTypeB &) = delete;
    VariantTypeB &operator =(VariantTypeB &&) = delete;
    void visit() {
        ++counts.b_visit;
    }
    ~VariantTypeB() {
        ++counts.b_destroyed;
    }
};

class VariantTypeC {
private:
    VariantVisitCounts &counts;
public:
    VariantTypeC(VariantVisitCounts &counts) : counts(counts) {}
    VariantTypeC(const VariantTypeC &) = delete;
    VariantTypeC(VariantTypeC &&mv) = delete;
    VariantTypeC &operator =(const VariantTypeC &) = delete;
    VariantTypeC &operator =(VariantTypeC &&) = delete;
    void visit() {
        ++counts.c_visit;
    }
    ~VariantTypeC() {
        ++counts.c_destroyed;
    }
};

void test_variant_visit_and_destroy() {
    VariantVisitCounts counts;
    {
        std::variant<VariantTypeA, VariantTypeB, VariantTypeC> v{std::in_place_type<VariantTypeA>, counts};
        assert(counts.a_destroyed == 0);
        std::visit([](auto &val) { val.visit(); }, v);
        assert(counts.a_visit == 1);
        assert(counts.b_visit == 0);
        assert(counts.c_visit == 0);
    }
    assert(counts.a_destroyed == 1);
    assert(counts.b_destroyed == 0);
    assert(counts.c_destroyed == 0);

    counts = {};
    {
        std::variant<VariantTypeA, VariantTypeB, VariantTypeC> v{std::in_place_type<VariantTypeB>, counts};
        assert(counts.b_destroyed == 0);
        std::visit([](auto &val) { val.visit(); }, v);
        assert(counts.a_visit == 0);
        assert(counts.b_visit == 1);
        assert(counts.c_visit == 0);
    }
    assert(counts.a_destroyed == 0);
    assert(counts.b_destroyed == 1);
    assert(counts.c_destroyed == 0);

    counts = {};
    {
        std::variant<VariantTypeA, VariantTypeB, VariantTypeC> v{std::in_place_type<VariantTypeC>, counts};
        assert(counts.c_destroyed == 0);
        std::visit([](auto &val) { val.visit(); }, v);
        assert(counts.a_visit == 0);
        assert(counts.b_visit == 0);
        assert(counts.c_visit == 1);
    }
    assert(counts.a_destroyed == 0);
    assert(counts.b_destroyed == 0);
    assert(counts.c_destroyed == 1);
}

int main() {
    test_variant_visit_and_destroy();
    return 0;
}
