//
// Created by sigsegv on 7/1/21.
//

#ifndef JEOKERNEL_ACPICA_H
#define JEOKERNEL_ACPICA_H

#include <cstdint>
#include <mutex>
#include <concurrency/critical_section.h>
#include <concurrency/hw_spinlock.h>
#include <functional>
#include <vector>
#include <sstream>
#include <thread>
#include <acpi/acpica_types.h>
#include <acpi/pci_irq_rt.h>
#include "acpi_madt_provider.h"

enum AcpiExecuteType
{
    GLOBAL_LOCK_HANDLER,
    NOTIFY_HANDLER,
    GPE_HANDLER,
    DEBUGGER_MAIN_THREAD,
    DEBUGGER_EXEC_THREAD,
    EC_POLL_HANDLER,
    EC_BURST_HANDLER
};

struct acpica_lock_context {
    virtual ~acpica_lock_context() {}
};

template <class lck> struct acpica_lock_context_impl : acpica_lock_context {
    critical_section cli;
    std::lock_guard<lck> lock;

    acpica_lock_context_impl(lck &l) : cli(), lock(l) {
    }
    ~acpica_lock_context_impl() {
    }
};

class acpibuffer {
public:
    void *ptr;
    size_t length;
    acpibuffer() : ptr(nullptr), length(0) {
    }
    acpibuffer(void *e_ptr, size_t length) : ptr(malloc(length)), length(length) {
        memcpy(ptr, e_ptr, length);
    }
    acpibuffer(const acpibuffer &buf) : acpibuffer(buf.ptr, buf.length) {
    }
    acpibuffer(acpibuffer &&mv) : ptr(mv.ptr), length(mv.length) {
        mv.ptr = nullptr;
    }
    ~acpibuffer() {
        if (ptr != nullptr) {
            free(ptr);
        }
    }
};

class acpica_lib : public acpi_madt_provider {
public:
    acpica_lib();
    virtual ~acpica_lib();

    bool set_ioapic_mode();
    void bootstrap();

    bool evaluate_integer(void *acpi_handle, const char *method, uint64_t &value);

    bool find_resources(void *handle, std::function<void (ACPI_RESOURCE *)> wfunc);
    bool find_pci_bridges(std::function<void (void *handle, ACPI_DEVICE_INFO *dev_info)> wfunc);

    bool walk_namespace(void *handle, std::function<void (std::string pathname, uint32_t object_type, void *handle)> wfunc);
    bool walk_namespace(std::function<void (std::string pathname, uint32_t object_type, void *handle)> wfunc);

    bool determine_pci_id(ACPI_PCI_ID &pciId, void *handle);
    bool determine_pci_id(ACPI_PCI_ID &pciId, const ACPI_DEVICE_INFO *dev_info, void *handle);

    acpibuffer get_irq_routing_table(void *handle);
    std::vector<PciIRQRouting> get_irq_routing(void *handle);

    std::optional<IRQLink> get_extended_irq(void *handle);

    std::shared_ptr<acpi_madt_info> get_madt() override;

    virtual void terminate() = 0;
    virtual void *allocate_zeroed(std::size_t size) = 0;
    virtual void *allocate(std::size_t size) = 0;
    virtual void free(void *ptr) = 0;
    virtual void *create_lock() = 0;
    virtual void destroy_lock(void *lock) = 0;
    virtual acpica_lock_context *acquire_lock(hw_spinlock &lock) = 0;
    void release_lock(acpica_lock_context *ctx) {
        delete ctx;
    }
    void *acquire_lock(void *lock) {
#if 0
        {
            std::stringstream ss{};
            ss << "Acquire lock " << std::hex << (uint64_t) lock << "\n";
            get_klogger() << ss.str().c_str();
        }
#endif
        hw_spinlock *lck = (hw_spinlock *) lock;
        acpica_lock_context *ctx = acquire_lock(*lck);
        return (void *) ctx;
    }
    void release_lock(void *lock, void *cpuflags) {
        acpica_lock_context *ctx = (acpica_lock_context *) cpuflags;
#if 0
        {
            std::stringstream ss{};
            ss << "Release lock " << std::hex << (uint64_t) lock <<" ctx=" << (uint64_t) cpuflags << "\n";
            get_klogger() << ss.str().c_str();
        }
#endif
        release_lock(ctx);
    }
    virtual void *create_semaphore(uint32_t max, uint32_t initial) = 0;
    virtual void destroy_semaphore(void *sema) = 0;
    virtual void wait_semaphore(void *sema, uint32_t units) = 0;
    virtual void signal_semaphore(void *sema, uint32_t units) = 0;
    virtual uint32_t get_thread_id() = 0;
    virtual void execute(AcpiExecuteType type, std::function<void ()> func) = 0;
    virtual void stall_nano(uint64_t nanos) = 0;
    virtual void sleep_nano(uint64_t nanos) = 0;
    virtual bool write_port(uint32_t addr, uint32_t value, uint32_t width) = 0;
    virtual bool read_port(uint32_t addr, uint32_t &value, uint32_t width) = 0;
    virtual bool write_memory(uint64_t addr, uint64_t value, uint32_t width) = 0;
    virtual bool read_memory(uint64_t addr, uint64_t &value, uint32_t width) = 0;
    virtual void install_int_handler(uint8_t intr, uint32_t (*handler)(void *), void *ctx) = 0;
    virtual void remove_int_handler(uint8_t intr, uint32_t (*hanlder)(void *)) = 0;
    virtual void wait_events_complete() = 0;
    virtual void *map_memory(uint64_t physaddr, size_t size) = 0;
    virtual void unmap_memory(void *addr, size_t size) = 0;
    virtual bool write_pci_reg(uint16_t segment, uint16_t bus, uint16_t device, uint16_t function, uint32_t reg, uint64_t value, uint32_t width) = 0;
    virtual bool read_pci_reg(uint16_t segment, uint16_t bus, uint16_t device, uint16_t function, uint32_t reg, uint64_t &value, uint32_t width) = 0;
    virtual uint64_t get_timer_100ns() = 0;
    virtual uint64_t get_root_addr() = 0;
};

struct acpica_int_handler {
    uint64_t handle;
    uint32_t (*handler)(void *);
    uint8_t intr;
};

class acpica_lib_impl : public acpica_lib {
private:
    hw_spinlock int_h_lock;
    std::vector<acpica_int_handler *> int_handlers;
    uint64_t root_table_addr;
public:
    acpica_lib_impl(uint64_t root_table_addr);
    ~acpica_lib_impl();

    void terminate() override;
    void *allocate_zeroed(std::size_t size) override;
    void *allocate(std::size_t size) override;
    void free(void *ptr) override;
    void *create_lock() override;
    void destroy_lock(void *lock) override;
    acpica_lock_context *acquire_lock(hw_spinlock &lock) override;
    void *create_semaphore(uint32_t max, uint32_t initial) override;
    void destroy_semaphore(void *sema) override;
    void wait_semaphore(void *sema, uint32_t units) override;
    void signal_semaphore(void *sema, uint32_t units) override;
    uint32_t get_thread_id() override;
    void execute(AcpiExecuteType type, std::function<void ()> func) override;
    void stall_nano(uint64_t nanos) override;
    void sleep_nano(uint64_t nanos) override;
    bool write_port(uint32_t addr, uint32_t value, uint32_t width) override;
    bool read_port(uint32_t addr, uint32_t &value, uint32_t width) override;
    bool write_memory(uint64_t addr, uint64_t value, uint32_t width) override;
    bool read_memory(uint64_t addr, uint64_t &value, uint32_t width) override;
    void install_int_handler(uint8_t intr, uint32_t (*handler)(void *), void *ctx) override;
    void remove_int_handler(uint8_t intr, uint32_t (*hanlder)(void *)) override;
    void wait_events_complete() override;
    void *map_memory(uint64_t physaddr, size_t size) override;
    void unmap_memory(void *addr, size_t size) override;
    bool write_pci_reg(uint16_t segment, uint16_t bus, uint16_t device, uint16_t function, uint32_t reg, uint64_t value, uint32_t width) override;
    bool read_pci_reg(uint16_t segment, uint16_t bus, uint16_t device, uint16_t function, uint32_t reg, uint64_t &value, uint32_t width) override;
    uint64_t get_timer_100ns() override;
    uint64_t get_root_addr() override;
};

acpica_lib &get_acpica();

#endif //JEOKERNEL_ACPICA_H
