//
// Created by sigsegv on 7/3/21.
//

#include <acpi/acpica.h>
#include <core/malloc.h>
#include <concurrency/hw_spinlock.h>
#include <concurrency/raw_semaphore.h>
#include <thread>
#include <core/nanotime.h>
#include <cpuio.h>
#include <core/vmem.h>
#include <pagealloc.h>
#include <sstream>
#include "../../HardwareInterrupts.h"
#include "../../pci/pci.h"
#include "../../CpuInterrupts.h"
#include <kernelconfig.h>

//#define PRINT_ACPICA_MEMMAPS
//#define DELAY_MEMMAPS

acpica_lib_impl::acpica_lib_impl(uint64_t root_table_addr) : acpica_lib(), int_h_lock(), int_handlers(), root_table_addr(root_table_addr) {
}

acpica_lib_impl::~acpica_lib_impl() {
}

void acpica_lib_impl::terminate() {

}

void *acpica_lib_impl::allocate_zeroed(std::size_t size) {
    void *buf = malloc(size);
    char *zbuf = (char *) buf;
    for (std::size_t i = 0; i < size; i++) {
        zbuf[i] = '\0';
    }
    return buf;
}

void *acpica_lib_impl::allocate(std::size_t size) {
#ifdef ACPICA_MALLOC_ZERO
    void *ptr = malloc(size);
    memset(ptr, 0, size);
    return ptr;
#else
    return malloc(size);
#endif
}

static void free_mem(void *ptr) {
    free(ptr);
}

void acpica_lib_impl::free(void *ptr) {
    free_mem(ptr);
}

void *acpica_lib_impl::create_lock() {
    return (void *) new hw_spinlock();
}

void acpica_lib_impl::destroy_lock(void *lock) {
    delete ((hw_spinlock *) lock);
}

void *acpica_lib_impl::create_semaphore(uint32_t max, uint32_t initial) {
    if (initial < 0x7FFFFFFF) {
        return (void *) new raw_semaphore(((int32_t) initial) - 1);
    } else {
        return nullptr;
    }
}

void acpica_lib_impl::destroy_semaphore(void *sema) {
    delete ((raw_semaphore *) sema);
}

uint32_t acpica_lib_impl::get_thread_id() {
    return get_scheduler()->get_current_task_id();
}

void acpica_lib_impl::wait_semaphore(void *sema, uint32_t units) {
    raw_semaphore *sem = (raw_semaphore *) sema;
    for (uint32_t i = 0; i < units; i++) {
        sem->acquire();
    }
}

void acpica_lib_impl::signal_semaphore(void *sema, uint32_t units) {
    raw_semaphore *sem = (raw_semaphore *) sema;
    for (uint32_t i = 0; i < units; i++) {
        sem->release();
    }
}

acpica_lock_context *acpica_lib_impl::acquire_lock(hw_spinlock &lock) {
    auto *ctx = new acpica_lock_context_impl(lock);
    return ctx;
}

void acpica_lib_impl::execute(AcpiExecuteType type, std::function<void()> func) {
    std::thread thread{[func] () mutable {
        std::this_thread::set_name("[acpi]");
        func();
    }};
    thread.detach();
}

void acpica_lib_impl::stall_nano(uint64_t nanos) {
    critical_section cli{};
    nanos += get_nanotime_ref();
    while (nanos > get_nanotime_ref()) {
        asm("pause");
    }
}

void acpica_lib_impl::sleep_nano(uint64_t nanos) {
    using namespace std::literals::chrono_literals;
    auto duration = operator ""ns(nanos);
    std::this_thread::sleep_for(duration);
}

bool acpica_lib_impl::write_port(uint32_t addr, uint32_t value, uint32_t width) {
    switch (width) {
        case 8: {
            outportb(addr, value);
        } break;
        case 16: {
            outportw(addr, value);
        } break;
        case 32: {
            outportl(addr, value);
        } break;
        default:
            return false;
    }
    return true;
}

bool acpica_lib_impl::read_port(uint32_t addr, uint32_t &value, uint32_t width) {
    switch (width) {
        case 8: {
            value = inportb(addr);
        } break;
        case 16: {
            value = inportw(addr);
        } break;
        case 32: {
            value = inportl(addr);
        } break;
        default:
            return false;
    }
    return true;
}

bool acpica_lib_impl::write_memory(uint64_t addr, uint64_t value, uint32_t width) {
    vmem vm{0x1000};
    uint64_t pageaddr = addr & 0xFFFFFFFFFFFFF000;
    uint64_t offset = addr & 0x0FFF;
    vm.page(0).rwmap(pageaddr, true, false);
    uint8_t *cptr = (uint8_t *) vm.pointer();
    cptr += offset;
    void *ptr = (void *) cptr;
    switch (width) {
        case 8: {
            *cptr = (uint8_t) value;
        } break;
        case 16: {
            *((uint16_t *) ptr) = (uint16_t) value;
        } break;
        case 32: {
            *((uint32_t *) ptr) = (uint32_t) value;
        } break;
        case 64: {
            *((uint64_t *) ptr) = (uint64_t) value;
        } break;
        default:
            return false;
    }
    return true;
}

bool acpica_lib_impl::read_memory(uint64_t addr, uint64_t &value, uint32_t width) {
    vmem vm{0x1000};
    uint64_t pageaddr = addr & 0xFFFFFFFFFFFFF000;
    uint64_t offset = addr & 0x0FFF;
    vm.page(0).rmap(pageaddr);
    uint8_t *cptr = (uint8_t *) vm.pointer();
    cptr += offset;
    void *ptr = (void *) cptr;
    switch (width) {
        case 8: {
            value = *cptr;
        } break;
        case 16: {
            value = *((uint16_t *) ptr);
        } break;
        case 32: {
            value = *((uint32_t *) ptr);
        } break;
        case 64: {
            value = *((uint64_t *) ptr);
        } break;
        default:
            return false;
    }
    return true;
}

void acpica_lib_impl::install_int_handler(uint8_t intr, uint32_t (*handler)(void *), void *ctx) {
    if (intr < 0x20) {
        acpica_int_handler *h = new acpica_int_handler;
        auto &cpu_ints = get_cpu_interrupt_handler();
        h->handler = handler;
        h->intr = intr;
        std::lock_guard lock{int_h_lock};
        h->handle = cpu_ints.add_handler(intr, [handler, ctx](Interrupt &intr) {
            handler(ctx);
        });
        int_handlers.push_back(h);
    } else if (intr >= 0x80) {
        {
            std::stringstream ss{};
            ss << "Requested handling of intr " << std::hex << (unsigned int) intr << "\n";
            get_klogger() << ss.str().c_str();
        }
        wild_panic("Intr handler setup out of range (0x20 - 0x7F)");
    } else {
        acpica_int_handler *h = new acpica_int_handler;
        auto &hw_ints = get_hw_interrupt_handler();
        h->handler = handler;
        h->intr = intr;
        std::lock_guard lock{int_h_lock};
        h->handle = hw_ints.add_handler(intr - 0x20, [handler, ctx](Interrupt &intr) {
            handler(ctx);
        });
        int_handlers.push_back(h);
    }
}

void acpica_lib_impl::remove_int_handler(uint8_t intr, uint32_t (*handler)(void *)) {
    std::vector<acpica_int_handler *> remove{};
    std::lock_guard lock{int_h_lock};
    if (intr < 0x20) {
        auto &cpu_ints = get_cpu_interrupt_handler();
        for (auto *h : int_handlers) {
            if (h->handler == handler && h->intr == intr) {
                cpu_ints.remove_handler(h->handle);
                remove.push_back(h);
            }
        }
    } else {
        auto &hw_ints = get_hw_interrupt_handler();
        for (auto *h : int_handlers) {
            if (h->handler == handler && h->intr == intr) {
                hw_ints.remove_handler(h->handle);
                remove.push_back(h);
            }
        }
    }
    for (auto *h : remove) {
        auto iterator = int_handlers.begin();
        while (iterator != int_handlers.end()) {
            if (*iterator == h) {
                int_handlers.erase(iterator);
            }
        }
    }
}

void acpica_lib_impl::wait_events_complete() {
    // TODO ??
}

void *acpica_lib_impl::map_memory(uint64_t physaddr, size_t size) {
    uint64_t offset = physaddr & 0x0FFF;
    uint64_t physpage = physaddr - offset;
    uint64_t realsize = offset + size;
    {
        uint64_t size_last_offset = realsize & 0x0FFF;
        if (size_last_offset > 0) {
            realsize += 0x1000 - size_last_offset;
        }
    }
    uint64_t vaddr = vpagealloc(realsize);
    uint64_t pages = realsize >> 12;
    uint64_t addr = 0;
    for (uint64_t page = 0; page < pages; page++) {
        auto pe = get_pageentr(vaddr + addr);
        pe->dirty = 0;
        pe->execution_disabled = 1;
        pe->writeable = 1;
        pe->accessed = 0;
        pe->user_access = 0;
        pe->write_through = 1;
        pe->cache_disabled = 1;
        pe->page_ppn = (physpage + addr) >> 12;
        pe->present = 1;
        update_pageentr(vaddr + addr, *pe);
        addr += 0x1000;
    }
#ifdef PRINT_ACPICA_MEMMAPS
    {
        std::stringstream ss{};
        ss << "ACPICA map " << std::hex << (uint64_t) physaddr << "(" << (uint64_t) size
           << ")" << (uint64_t) physpage << " ==> " << (uint64_t) vaddr << "+" << (uint64_t) offset << "\n";
        get_klogger() << ss.str().c_str();
    }
#endif
    reload_pagetables();
#ifdef DELAY_MEMMAPS
    stall_nano(10000000);
#endif
    vaddr += offset;
    return ((void *) vaddr);
}

void acpica_lib_impl::unmap_memory(void *addr, size_t size) {
    uint64_t offset = ((uint64_t) addr) & 0x0FFF;
    uint64_t pageaddr = ((uint64_t) addr) - offset;
#ifdef PRINT_ACPICA_MEMMAPS
    {
        std::stringstream ss{};
        ss << "ACPICA unmap " << std::hex << (uint64_t) addr << " => " << (uint64_t) pageaddr << "\n";
        get_klogger() << ss.str().c_str();
    }
#endif
    vpagefree(pageaddr);
    reload_pagetables();
}

bool acpica_lib_impl::write_pci_reg(uint16_t segment, uint16_t bus, uint16_t device, uint16_t function, uint32_t reg,
                                       uint64_t value, uint32_t width) {
    switch (width) {
        case 8: {
            write8_pci_config(bus, device, function, reg, value);
        } break;
        case 16: {
            wild_panic("acpica: write pci 16bit reg not implemented");
        } break;
        case 32: {
            write_pci_config(bus, device, function, reg, value);
        } break;
        case 64: {
            wild_panic("acpica: write pci 64bit reg not implemented");
        } break;
        default:
            wild_panic("acpica: write pci // width??");
            return false;
    }
    return false;
}

bool acpica_lib_impl::read_pci_reg(uint16_t segment, uint16_t bus, uint16_t device, uint16_t function, uint32_t reg,
                                    uint64_t &value, uint32_t width) {
    switch (width) {
        case 8: {
            value = read8_pci_config(bus, device, function, reg);
        } break;
        case 16: {
            wild_panic("acpica: read pci 16bit reg not implemented");
        } break;
        case 32: {
            value = read_pci_config(bus, device, function, reg);
        } break;
        case 64: {
            wild_panic("acpica: read pci 64bit reg not implemented");
        } break;
        default:
            wild_panic("acpica: read pci // width??");
            return false;
    }
    return true;
}

uint64_t acpica_lib_impl::get_timer_100ns() {
    return get_ticks() * 100000;
}

uint64_t acpica_lib_impl::get_root_addr() {
    return root_table_addr;
}



