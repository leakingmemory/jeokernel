//
// Created by sigsegv on 9/23/26.
//

#include "dtb.h"

namespace {
    constexpr uint32_t FDT_MAGIC = 0xd00dfeed;
    constexpr uint32_t FDT_BEGIN_NODE = 0x1;
    constexpr uint32_t FDT_END_NODE = 0x2;
    constexpr uint32_t FDT_PROP = 0x3;
    constexpr uint32_t FDT_NOP = 0x4;
    constexpr uint32_t FDT_END = 0x9;

    uint32_t be32(const uint8_t *p) {
        return (static_cast<uint32_t>(p[0]) << 24) |
               (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8) |
               static_cast<uint32_t>(p[3]);
    }

    uint64_t read_cells(const uint8_t *p, uint32_t cells) {
        uint64_t v = 0;
        for (uint32_t i = 0; i < cells; ++i, p += 4) {
            v = (v << 32) | be32(p);
        }
        return v;
    }

    bool streq(const char *a, const char *b) {
        while (*a != '\0' && *a == *b) {
            ++a;
            ++b;
        }
        return *a == *b;
    }

    bool strstarts(const char *str, const char *prefix) {
        while (*prefix != '\0') {
            if (*str != *prefix) {
                return false;
            }
            ++str;
            ++prefix;
        }
        return true;
    }

    const uint8_t *align4(const uint8_t *p) {
        return reinterpret_cast<const uint8_t *>((reinterpret_cast<uintptr_t>(p) + 3) & ~static_cast<uintptr_t>(3));
    }

    uint64_t virt_to_phys(uint64_t vaddr) {
        uint64_t par;
        asm volatile(
            "at s1e1r, %1\n"
            "isb\n"
            "mrs %0, par_el1\n"
            : "=r"(par)
            : "r"(vaddr)
            : "memory"
        );
        if ((par & 1) == 0) {
            return (par & 0x0000fffffffff000ULL) | (vaddr & 0xFFFULL);
        }
        return 0;
    }
}

dtb::dtb(uint64_t dtb_vaddr) : blob(reinterpret_cast<const uint8_t *>(dtb_vaddr)), total_size(0), valid(false) {
    if (blob != nullptr && be32(blob) == FDT_MAGIC) {
        total_size = be32(blob + 4);
        valid = true;
    }
}

std::vector<memory_region> dtb::get_memory_banks() const {
    std::vector<memory_region> banks{};
    if (!valid || blob == nullptr) {
        return banks;
    }

    const uint32_t off_struct = be32(blob + 8);
    const uint32_t off_strings = be32(blob + 12);
    const char *strings = reinterpret_cast<const char *>(blob + off_strings);
    const uint8_t *p = blob + off_struct;

    uint32_t addr_cells = 2;
    uint32_t size_cells = 1;

    int depth = 0;
    bool is_memory = false;
    const uint8_t *reg = nullptr;
    uint32_t reg_len = 0;

    for (;;) {
        const uint32_t tok = be32(p);
        p += 4;

        if (tok == FDT_END) {
            break;
        } else if (tok == FDT_NOP) {
            continue;
        } else if (tok == FDT_BEGIN_NODE) {
            ++depth;
            const char *nodename = reinterpret_cast<const char *>(p);
            while (*p != '\0') {
                ++p;
            }
            p = align4(p + 1);

            is_memory = (strstarts(nodename, "memory@") || streq(nodename, "memory"));
            reg = nullptr;
            reg_len = 0;
        } else if (tok == FDT_END_NODE) {
            if (is_memory && reg != nullptr) {
                const uint32_t entry = (addr_cells + size_cells) * 4;
                for (uint32_t o = 0; o + entry <= reg_len; o += entry) {
                    const uint64_t base = read_cells(reg + o, addr_cells);
                    const uint64_t size = read_cells(reg + o + addr_cells * 4, size_cells);
                    if (size > 0) {
                        banks.push_back({base, size});
                    }
                }
            }
            --depth;
            is_memory = false;
            reg = nullptr;
        } else if (tok == FDT_PROP) {
            const uint32_t len = be32(p);
            const uint32_t nameoff = be32(p + 4);
            const uint8_t *val = p + 8;
            p = align4(p + 8 + len);

            const char *name = strings + nameoff;
            if (depth == 1 && streq(name, "#address-cells")) {
                addr_cells = be32(val);
            } else if (depth == 1 && streq(name, "#size-cells")) {
                size_cells = be32(val);
            } else if (streq(name, "device_type") && streq(reinterpret_cast<const char *>(val), "memory")) {
                is_memory = true;
            } else if (streq(name, "reg")) {
                reg = val;
                reg_len = len;
            }
        } else {
            break;
        }
    }

    return banks;
}

std::vector<memory_region> dtb::get_reserved_regions() const {
    std::vector<memory_region> reserved{};
    if (!valid || blob == nullptr) {
        return reserved;
    }

    // 1. Memreserve list from header
    const uint32_t off_mem_rsvmap = be32(blob + 16);
    const uint8_t *rsv = blob + off_mem_rsvmap;
    while (rsv + 16 <= blob + total_size) {
        const uint64_t rsv_addr = (static_cast<uint64_t>(be32(rsv)) << 32) | be32(rsv + 4);
        const uint64_t rsv_size = (static_cast<uint64_t>(be32(rsv + 8)) << 32) | be32(rsv + 12);
        rsv += 16;
        if (rsv_addr == 0 && rsv_size == 0) {
            break;
        }
        if (rsv_size > 0) {
            reserved.push_back({rsv_addr, rsv_size});
        }
    }

    // 2. DTB itself
    uint64_t dtb_phys = virt_to_phys(reinterpret_cast<uint64_t>(blob));
    if (dtb_phys != 0 && total_size > 0) {
        reserved.push_back({dtb_phys, total_size});
    }

    // 3. /reserved-memory nodes
    const uint32_t off_struct = be32(blob + 8);
    const uint32_t off_strings = be32(blob + 12);
    const char *strings = reinterpret_cast<const char *>(blob + off_strings);
    const uint8_t *p = blob + off_struct;

    uint32_t root_addr_cells = 2;
    uint32_t root_size_cells = 1;
    uint32_t rsv_addr_cells = 2;
    uint32_t rsv_size_cells = 1;

    int depth = 0;
    bool in_reserved_memory_parent = false;
    bool in_reserved_memory_child = false;
    const uint8_t *reg = nullptr;
    uint32_t reg_len = 0;

    for (;;) {
        const uint32_t tok = be32(p);
        p += 4;

        if (tok == FDT_END) {
            break;
        } else if (tok == FDT_NOP) {
            continue;
        } else if (tok == FDT_BEGIN_NODE) {
            ++depth;
            const char *nodename = reinterpret_cast<const char *>(p);
            while (*p != '\0') {
                ++p;
            }
            p = align4(p + 1);

            if (depth == 1 && (streq(nodename, "reserved-memory") || strstarts(nodename, "reserved-memory@"))) {
                in_reserved_memory_parent = true;
                rsv_addr_cells = root_addr_cells;
                rsv_size_cells = root_size_cells;
            } else if (depth == 2 && in_reserved_memory_parent) {
                in_reserved_memory_child = true;
            }
            reg = nullptr;
            reg_len = 0;
        } else if (tok == FDT_END_NODE) {
            if (in_reserved_memory_child && reg != nullptr) {
                const uint32_t entry = (rsv_addr_cells + rsv_size_cells) * 4;
                for (uint32_t o = 0; o + entry <= reg_len; o += entry) {
                    const uint64_t base = read_cells(reg + o, rsv_addr_cells);
                    const uint64_t size = read_cells(reg + o + rsv_addr_cells * 4, rsv_size_cells);
                    if (size > 0) {
                        reserved.push_back({base, size});
                    }
                }
            }
            if (depth == 2) {
                in_reserved_memory_child = false;
            } else if (depth == 1) {
                in_reserved_memory_parent = false;
            }
            --depth;
            reg = nullptr;
        } else if (tok == FDT_PROP) {
            const uint32_t len = be32(p);
            const uint32_t nameoff = be32(p + 4);
            const uint8_t *val = p + 8;
            p = align4(p + 8 + len);

            const char *name = strings + nameoff;
            if (depth == 1) {
                if (streq(name, "#address-cells")) {
                    root_addr_cells = be32(val);
                } else if (streq(name, "#size-cells")) {
                    root_size_cells = be32(val);
                }
            } else if (depth == 2 && in_reserved_memory_parent) {
                if (streq(name, "#address-cells")) {
                    rsv_addr_cells = be32(val);
                } else if (streq(name, "#size-cells")) {
                    rsv_size_cells = be32(val);
                }
            }
            if (in_reserved_memory_child && streq(name, "reg")) {
                reg = val;
                reg_len = len;
            }
        } else {
            break;
        }
    }

    return reserved;
}
