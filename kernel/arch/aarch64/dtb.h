//
// Created by sigsegv on 9/23/26.
//

#ifndef JEOKERNEL_AARCH64_DTB_H
#define JEOKERNEL_AARCH64_DTB_H

#include <cstdint>
#include <vector>

struct memory_region {
    uint64_t base;
    uint64_t size;
};

class dtb {
private:
    const uint8_t *blob;
    uint32_t total_size;
    bool valid;

public:
    explicit dtb(uint64_t dtb_vaddr);

    [[nodiscard]] bool is_valid() const {
        return valid;
    }

    [[nodiscard]] uint32_t get_total_size() const {
        return total_size;
    }

    [[nodiscard]] const uint8_t *get_blob() const {
        return blob;
    }

    [[nodiscard]] std::vector<memory_region> get_memory_banks() const;
    [[nodiscard]] std::vector<memory_region> get_reserved_regions() const;
};

#endif //JEOKERNEL_AARCH64_DTB_H
