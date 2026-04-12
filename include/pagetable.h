//
// Created by sigsegv on 17.04.2021.
//

#ifndef JEOKERNEL_PAGETABLE_H
#define JEOKERNEL_PAGETABLE_H

#include <stdint.h>

#ifndef UNIT_TESTING
#include <std/compilerwarnings.h>
#endif

#define PAGESIZE 4096

uintptr_t get_pagetable_virt_offset();
void set_pagetable_virt_offset(uintptr_t offset);

template <const int S> constexpr static uint8_t uint8_mask_for_bits() {
    static_assert(S > 0 && S <= 8);
    switch (S) {
        case 1:
            return 1;
        case 2:
            return 3;
        case 3:
            return 7;
        case 4:
            return 15;
        case 5:
            return 31;
        case 6:
            return 63;
        case 7:
            return 127;
        case 8:
            return 255;
    }
}
template <const int S> constexpr static uint16_t uint16_mask_for_bits() {
    if constexpr(S < 9) {
        return uint8_mask_for_bits<S>();
    } else {
        static_assert(S > 8 && S <= 16);
        switch (S) {
            case 9:
                return 0x1FF;
            case 10:
                return 0x3FF;
            case 11:
                return 0x7FF;
            case 12:
                return 0xFFF;
            case 13:
                return 0x1FFF;
            case 14:
                return 0x3FFF;
            case 15:
                return 0x7FFF;
            case 16:
                return 0xFFFF;
        }
    }
}
template <const int S> constexpr static uint32_t uint32_mask_for_bits() {
    if constexpr(S < 17) {
        return uint16_mask_for_bits<S>();
    } else {
        static_assert(S > 16 && S <= 32);
        switch (S) {
            case 17:
                return 0x1FFFF;
            case 18:
                return 0x3FFFF;
            case 19:
                return 0x7FFFF;
            case 20:
                return 0xFFFFF;
            case 21:
                return 0x1FFFFF;
            case 22:
                return 0x3FFFFF;
            case 23:
                return 0x7FFFFF;
            case 24:
                return 0xFFFFFF;
            case 25:
                return 0x1FFFFFF;
            case 26:
                return 0x3FFFFFF;
            case 27:
                return 0x7FFFFFF;
            case 28:
                return 0xFFFFFFF;
            case 29:
                return 0x1FFFFFFF;
            case 30:
                return 0x3FFFFFFF;
            case 31:
                return 0x7FFFFFFF;
            case 32:
                return 0xFFFFFFFF;
        }
    }
}
template <const int S> constexpr static uint64_t uint64_mask_for_bits() {
    if constexpr(S < 33) {
        return uint32_mask_for_bits<S>();
    } else {
        static_assert(S > 32 && S <= 64);
        switch (S) {
            case 33:
                return 0x1FFFFFFFFULL;
            case 34:
                return 0x3FFFFFFFFULL;
            case 35:
                return 0x7FFFFFFFFULL;
            case 36:
                return 0xFFFFFFFFFULL;
            case 37:
                return 0x1FFFFFFFFFULL;
            case 38:
                return 0x3FFFFFFFFFULL;
            case 39:
                return 0x7FFFFFFFFFULL;
            case 40:
                return 0xFFFFFFFFFFULL;
            case 41:
                return 0x1FFFFFFFFFFULL;
            case 42:
                return 0x3FFFFFFFFFFULL;
            case 43:
                return 0x7FFFFFFFFFFULL;
            case 44:
                return 0xFFFFFFFFFFFULL;
            case 45:
                return 0x1FFFFFFFFFFFULL;
            case 46:
                return 0x3FFFFFFFFFFFULL;
            case 47:
                return 0x7FFFFFFFFFFFULL;
            case 48:
                return 0xFFFFFFFFFFFFULL;
            case 49:
                return 0x1FFFFFFFFFFFFULL;
            case 50:
                return 0x3FFFFFFFFFFFFULL;
            case 51:
                return 0x7FFFFFFFFFFFFULL;
            case 52:
                return 0xFFFFFFFFFFFFFULL;
            case 53:
                return 0x1FFFFFFFFFFFFFULL;
            case 54:
                return 0x3FFFFFFFFFFFFFULL;
            case 55:
                return 0x7FFFFFFFFFFFFFULL;
            case 56:
                return 0xFFFFFFFFFFFFFFULL;
            case 57:
                return 0x1FFFFFFFFFFFFFFULL;
            case 58:
                return 0x3FFFFFFFFFFFFFFULL;
            case 59:
                return 0x7FFFFFFFFFFFFFFULL;
            case 60:
                return 0xFFFFFFFFFFFFFFFULL;
            case 61:
                return 0x1FFFFFFFFFFFFFFFULL;
            case 62:
                return 0x3FFFFFFFFFFFFFFFULL;
            case 63:
                return 0x7FFFFFFFFFFFFFFFULL;
            case 64:
                return 0xFFFFFFFFFFFFFFFFULL;
        }
    }
}

template <typename T, const int N, const int S = 1> class ptbl_ReadBitfieldOfValue {
private:
    const T &value;
public:
    constexpr ptbl_ReadBitfieldOfValue(const T &value) : value(value) {}
    constexpr operator uint8_t() const {
        return (value >> N) & uint8_mask_for_bits<S>();
    }
};

template <typename T, const int N, const int S = 1> class ptbl_BitfieldOfValue {
private:
    T &value;
public:
    constexpr ptbl_BitfieldOfValue(T &value) : value(value) {}
    constexpr operator uint8_t() const {
        return (value >> N) & uint8_mask_for_bits<S>();
    }
    constexpr operator bool() const {
        return operator uint8_t() != 0;
    }
    constexpr uint8_t operator = (uint8_t val) {
        value = (value & ~(static_cast<T>(uint8_mask_for_bits<S>()) << N)) | (static_cast<T>(val & uint8_mask_for_bits<S>()) << N);
        return val & uint8_mask_for_bits<S>();
    }
};

template <typename T, const int N, const int S = 1> class ptbl16_ReadBitfieldOfValue {
private:
    const T &value;
public:
    constexpr ptbl16_ReadBitfieldOfValue(const T &value) : value(value) {}
    constexpr operator uint16_t() const {
        return (value >> N) & uint16_mask_for_bits<S>();
    }
};

template <typename T, const int N, const int S = 1> class ptbl16_BitfieldOfValue {
private:
    T &value;
public:
    constexpr ptbl16_BitfieldOfValue(T &value) : value(value) {}
    constexpr operator uint16_t() const {
        return (value >> N) & uint16_mask_for_bits<S>();
    }
    constexpr uint8_t operator = (uint16_t val) {
        value = (value & ~(static_cast<T>(uint16_mask_for_bits<S>()) << N)) | (static_cast<T>(val & uint16_mask_for_bits<S>()) << N);
        return val & uint16_mask_for_bits<S>();
    }
};

template <typename T, const int N, const int S = 1> class ptbl32_ReadBitfieldOfValue {
private:
    const T &value;
public:
    constexpr ptbl32_ReadBitfieldOfValue(const T &value) : value(value) {}
    constexpr operator uint32_t() const {
        return (value >> N) & uint32_mask_for_bits<S>();
    }
};

template <typename T, const int N, const int S = 1> class ptbl32_BitfieldOfValue {
private:
    T &value;
public:
    constexpr ptbl32_BitfieldOfValue(T &value) : value(value) {}
    constexpr operator uint32_t() const {
        return (value >> N) & uint32_mask_for_bits<S>();
    }
    constexpr uint8_t operator = (uint32_t val) {
        value = (value & ~(static_cast<T>(uint32_mask_for_bits<S>()) << N)) | (static_cast<T>(val & uint32_mask_for_bits<S>()) << N);
        return val & uint32_mask_for_bits<S>();
    }
};

template <typename T, const int N, const int S = 1> class ptbl64_ReadBitfieldOfValue {
private:
    const T &value;
public:
    constexpr ptbl64_ReadBitfieldOfValue(const T &value) : value(value) {}
    constexpr operator uint64_t() const {
        return (value >> N) & uint64_mask_for_bits<S>();
    }
};

template <typename T, const int N, const int S = 1> class ptbl64_BitfieldOfValue {
private:
    T &value;
public:
    constexpr ptbl64_BitfieldOfValue(T &value) : value(value) {}
    constexpr operator uint64_t() const {
        return (value >> N) & uint64_mask_for_bits<S>();
    }
    constexpr uint64_t operator = (uint64_t val) {
        value = (value & (static_cast<T>(~(uint64_mask_for_bits<S>()) << N))) | (static_cast<T>(val & uint64_mask_for_bits<S>()) << N);
        return val & uint64_mask_for_bits<S>();
    }
};

struct GDT {
    uint64_t value;

    constexpr const ptbl16_ReadBitfieldOfValue<uint64_t,0,16> limit_low() const {
        return {value};
    }
    constexpr const ptbl32_ReadBitfieldOfValue<uint64_t,16,24> base_low() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,40,8> type() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,48,4> limit_high() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,52,4> granularity() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,56,8> base_high() const {
        return {value};
    }

    constexpr ptbl16_BitfieldOfValue<uint64_t,0,16> limit_low() {
        return {value};
    }
    constexpr ptbl32_BitfieldOfValue<uint64_t,16,24> base_low() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,40,8> type() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,48,4> limit_high() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,52,4> granularity() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,56,8> base_high() {
        return {value};
    }

    constexpr GDT() : value(0) {
        limit_low() = 0xFFFF;
        limit_high() = 1;
    }
    constexpr GDT(uint32_t base, uint32_t limit, uint8_t granularity, uint8_t type) : value(0) {
        limit_low() = limit & 0xFFFF;
        base_low() = base & 0xFFFFFF;
        this->type() = type;
        limit_high() = (limit >> 16) & 0x0F;
        this->granularity() = granularity & 0x0F;
        base_high() = base >> 24;
    }

    void set_base(uint32_t base_addr) {
        base_low() = base_addr & 0xFFFFFF;
        base_high() = (base_addr >> 24);
    }
    void set_limit(uint32_t limit) {
        limit_low() = limit & 0xFFFF;
        limit_high() = (limit >> 16) & 0x0F;
    }
} __attribute__((__packed__));

static_assert(GDT(0, 0x1FFFF, 0, 0).value == 0x000100000000FFFFULL);
static_assert(GDT(0, 0x1FFFF, 0, 0).base_high().operator uint8_t() == 0);
static_assert(GDT(0, 0x1FFFF, 0, 0).base_low().operator uint32_t() == 0);
static_assert(GDT(0, 0x1FFFF, 0, 0).limit_high().operator uint8_t() == 1);
static_assert(GDT(0, 0x1FFFF, 0, 0).limit_low().operator uint16_t() == 0xFFFF);
static_assert(GDT(0, 0x1FFFF, 0, 0).granularity().operator uint8_t() == 0);
static_assert(GDT(0, 0x1FFFF, 0, 0).type().operator uint8_t() == 0);

static_assert(GDT(0, 0xF0000, 0xA, 0x9A).value == 0x00AF9A0000000000ULL);
static_assert(GDT(0, 0xF0000, 0xA, 0x9A).base_high().operator uint8_t() == 0);
static_assert(GDT(0, 0xF0000, 0xA, 0x9A).base_low().operator uint32_t() == 0);
static_assert(GDT(0, 0xF0000, 0xA, 0x9A).limit_high().operator uint8_t() == 0xF);
static_assert(GDT(0, 0xF0000, 0xA, 0x9A).limit_low().operator uint16_t() == 0);
static_assert(GDT(0, 0xF0000, 0xA, 0x9A).granularity().operator uint8_t() == 0xA);
static_assert(GDT(0, 0xF0000, 0xA, 0x9A).type().operator uint8_t() == 0x9A);

static_assert(GDT(0, 0, 0, 0x92).value == 0x0000920000000000ULL);
static_assert(GDT(0, 0, 0, 0x92).base_high().operator uint8_t() == 0);
static_assert(GDT(0, 0, 0, 0x92).base_low().operator uint32_t() == 0);
static_assert(GDT(0, 0, 0, 0x92).limit_high().operator uint8_t() == 0);
static_assert(GDT(0, 0, 0, 0x92).limit_low().operator uint16_t() == 0);
static_assert(GDT(0, 0, 0, 0x92).granularity().operator uint8_t() == 0);
static_assert(GDT(0, 0, 0, 0x92).type().operator uint8_t() == 0x92);

struct LDTP {
    uint64_t value1;
    uint64_t value2;

    constexpr const ptbl16_ReadBitfieldOfValue<uint64_t,0,16> limit_low() const {
        return {value1};
    }
    constexpr const ptbl32_ReadBitfieldOfValue<uint64_t,16,24> base_low() const {
        return {value1};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,40,8> type() const {
        return {value1};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,48,4> limit_high() const {
        return {value1};
    }
    constexpr const ptbl16_ReadBitfieldOfValue<uint64_t,52,4> granularity() const {
        return {value1};
    }
    constexpr const ptbl64_ReadBitfieldOfValue<uint64_t,56,8> base_high_low() const {
        return {value1};
    }
    constexpr const ptbl64_ReadBitfieldOfValue<uint64_t,0,32> base_high_high() const {
        return {value2};
    }
    constexpr const ptbl32_ReadBitfieldOfValue<uint64_t,32,32> reserved() const {
        return {value2};
    }

    constexpr ptbl16_BitfieldOfValue<uint64_t,0,16> limit_low() {
        return {value1};
    }
    constexpr ptbl32_BitfieldOfValue<uint64_t,16,24> base_low() {
        return {value1};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,40,8> type() {
        return {value1};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,48,4> limit_high() {
        return {value1};
    }
    constexpr ptbl16_BitfieldOfValue<uint64_t,52,4> granularity() {
        return {value1};
    }
    constexpr ptbl64_BitfieldOfValue<uint64_t,56,8> base_high_low() {
        return {value1};
    }
    constexpr ptbl64_BitfieldOfValue<uint64_t,0,32> base_high_high() {
        return {value2};
    }
    constexpr ptbl32_BitfieldOfValue<uint64_t,32,32> reserved() {
        return {value2};
    }

    void set_base(uint64_t base_addr) {
        base_low() = base_addr & 0xFFFFFF;
        base_high_low() = (base_addr >> 24) & 0xFF;
        base_high_high() = base_addr >> 32;
    }
    void set_limit(uint32_t limit) {
        limit_low() = limit & 0xFFFF;
        limit_high() = (limit >> 16) & 0x0F;
    }
} __attribute__((__packed__));
static_assert(sizeof(LDTP) == (2*sizeof(GDT)));

struct GDT_addr_64_in_32 {
    uint32_t low;
    uint32_t high;
};

union GDT_addr {
    GDT_addr_64_in_32 addr64in32;
    uint64_t addr;
};
static_assert(sizeof(GDT_addr) == 8);

template <int n> struct GDT_table {
    static_assert(n > 0);
    GDT gdt[n];
    uint16_t size;
    GDT_addr ptr;

    GDT_table() {
        for (int i = 0; i < n; i++) {
            gdt[i] = GDT();
        }
    }

    void set_size() {
        size = sizeof(gdt) - 1;
    }
    void set_ptr() {
        ptr.addr = (uint64_t) this;
    }

    GDT & operator [] (int i) {
        if (i < 0) {
            return gdt[0];
        }
        if (i < n) {
            return gdt[i];
        } else {
            return gdt[n -1];
        }
    }

    uint64_t pointer() {
        set_size();
        set_ptr();

#ifndef UNIT_TESTING
COMPILER_WARNINGS_PUSH()
GNU_AND_CLANG_IGNORE_WARNING(-Waddress-of-packed-member)
#endif
        return (uint64_t) &size;
#ifndef UNIT_TESTING
COMPILER_WARNINGS_POP()
#endif
    }
} __attribute__((__packed__));

static_assert(sizeof(GDT) == 8);

struct pageentr {
    uint64_t value;

    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,0> present() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,1> writeable() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,2> user_access() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,3> write_through() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,4> cache_disabled() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,5> accessed() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,6> dirty() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,7> size() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,8> global() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,9,2> ignored2() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,11> os_virt_avail() const {
        return {value};
    }
    constexpr const ptbl32_ReadBitfieldOfValue<uint64_t,12,28> page_ppn() const {
        return {value};
    }
    constexpr const ptbl16_ReadBitfieldOfValue<uint64_t,40,12> reserved1() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,52> os_virt_start() const {
        return {value};
    }
    constexpr const ptbl16_ReadBitfieldOfValue<uint64_t,53,10> ignored3() const {
        return {value};
    }
    constexpr const ptbl_ReadBitfieldOfValue<uint64_t,63> execution_disabled() const {
        return {value};
    }

    constexpr ptbl_BitfieldOfValue<uint64_t,0> present() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,1> writeable() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,2> user_access() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,3> write_through() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,4> cache_disabled() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,5> accessed() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,6> dirty() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,7> size() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,8> global() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,9,2> ignored2() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,11> os_virt_avail() {
        return {value};
    }
    constexpr ptbl32_BitfieldOfValue<uint64_t,12,28> page_ppn() {
        return {value};
    }
    constexpr ptbl16_BitfieldOfValue<uint64_t,40,12> reserved1() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,52> os_virt_start() {
        return {value};
    }
    constexpr ptbl16_BitfieldOfValue<uint64_t,53,10> ignored3() {
        return {value};
    }
    constexpr ptbl_BitfieldOfValue<uint64_t,63> execution_disabled() {
        return {value};
    }

    uint64_t get_subtable_addr() const {
        uint64_t addr = page_ppn();
        addr *= 4096;
        return addr + get_pagetable_virt_offset();
    }
    typedef pageentr pdpt[512];
    pdpt &get_subtable() const {
        return *((pdpt *) get_subtable_addr());
    }
} __attribute__((__packed__));

static_assert(sizeof(pageentr) == 8);

typedef pageentr pagetable[512];

pageentr &get_pml4t_pageentr64(pagetable &pml4t, uint64_t addr);
pageentr &get_pdpt_pageentr64(pagetable &pdpt_ref, uint64_t addr);
pageentr &get_pdt_pageentr64(pagetable &pdt_ref, uint64_t addr);
pageentr &get_pt_pageentr64(pagetable &pt_ref, uint64_t addr);
pageentr *get_pageentr64(pagetable &pml4t, uint64_t addr);

#ifndef LOADER

#include <optional>
#include <functional>

#ifndef UNIT_TESTING
#include <concurrency/hw_spinlock.h>

#endif

void initialize_pagetable_control();

hw_spinlock &get_pagetables_lock();

uint64_t get_phys_from_virt(uint64_t vaddr);
std::optional<pageentr> get_pageentr(uint64_t addr);
/**
 * Update the vmem properties of the pageentr. Any allocation
 * bits, reserved bits, and similar will be left unchanged.
 *
 * @param addr
 * @param pe_vmem_update
 * @return True on success.
 */
bool update_pageentr(uint64_t addr, const pageentr &pe_vmem_update);
bool update_pageentr(uint64_t addr, std::function<void (pageentr &pe)>);

#endif
#endif //JEOKERNEL_PAGETABLE_H
