//
// Created by sigsegv on 25.04.2021.
//

#ifndef JEOKERNEL_INTERRUPTDESCRIPTORTABLE_H
#define JEOKERNEL_INTERRUPTDESCRIPTORTABLE_H

#include <stdint.h>

struct IDTDescr {
    uint16_t offset_low;
    uint16_t code_segment;
    uint8_t interrupt_stack_table_offset : 2;
    uint8_t reserved1 : 6;
    uint8_t gate_type : 4;
    uint8_t storage_segment : 1;
    uint8_t callers_minimum_privilege_level : 2;
    uint8_t present : 1;
    uint64_t offset_high : 48;
    uint32_t reserved2;

    void set_offset(uint64_t offset) {
        offset_low = offset & 0xFFFF;
        offset_high = offset >> 16;
    }
} __attribute__((__packed__));
static_assert(sizeof(IDTDescr) == 16);

struct IDTPointer {
    uint16_t size;
    uint64_t addr;
} __attribute__((__packed__));

template <int n> struct IDTTable {
    IDTDescr idts[n];
    IDTPointer pointer;

    IDTTable(phys_t addr) {
        memset(&(idts[0]), 0, sizeof(idts));
        pointer.size = sizeof(idts) - 1;
        pointer.addr = addr;
    }
    IDTTable() : IDTTable((uint64_t) &(idts[0])) {
    }
    bool is_valid_index(int idx) const {
        return idx >= 0 && idx < n;
    }
    phys_t PointerOffset() {
        return (((uintptr_t) &pointer) - ((uintptr_t) this));
    }
};

class InterruptDescriptorTable {
private:
    IDTTable<256> *idt_table;
    phys_t physptr;
public:
    InterruptDescriptorTable();
    /**
     * Deleting this object while the IDT is loaded is obviously UB
     */
    ~InterruptDescriptorTable();

    /*
     * Constructed in place and stays in place. No copies, moves.
     */
    InterruptDescriptorTable(const InterruptDescriptorTable &) = delete;
    InterruptDescriptorTable(InterruptDescriptorTable &&) = delete;
    InterruptDescriptorTable & operator = (const InterruptDescriptorTable &) = delete;
    InterruptDescriptorTable & operator = (InterruptDescriptorTable &&) = delete;

    void interrupt_handler(
            uint16_t tbl_index,
            uint16_t code_segment, uint64_t offset, uint8_t interrupt_stack_table_offset,
            uint8_t gate_type, uint8_t callers_minimum_privilege
            ) {
        if (idt_table->is_valid_index(tbl_index)) {
            auto &idt = idt_table->idts[tbl_index];
            idt.code_segment = code_segment;
            idt.set_offset(offset);
            idt.interrupt_stack_table_offset = interrupt_stack_table_offset;
            idt.gate_type = gate_type;
            idt.callers_minimum_privilege_level = callers_minimum_privilege;
            idt.present = 1;
            idt.storage_segment = 0;
        }
    }

    void install();

    const IDTTable<256> &idt() {
        return *idt_table;
    }
};


#endif //JEOKERNEL_INTERRUPTDESCRIPTORTABLE_H
