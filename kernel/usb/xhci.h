//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_XHCI_H
#define JEOKERNEL_XHCI_H

#ifndef UNIT_TESTING
#include <devices/drivers.h>
#include <core/vmem.h>
#include "../pci/pci.h"
#include "usb_hcd.h"

#endif

#include <sstream>
#include <memory>

struct xhci_reg64 {
    uint32_t low;
    uint32_t high;

    operator uint64_t() {
        uint32_t low{this->low};
        uint64_t value{this->high};
        value = value << 32;
        value |= low;
        return value;
    }
    uint64_t operator = (uint64_t value) {
        low = (uint32_t) value;
        high = (uint32_t) (value >> 32);
        return value;
    }
} __attribute__((__packed__));

struct HostControllerPortRegisters {
    uint32_t portStatusControl;
    uint32_t portPowerManagementStatusControl;
    uint32_t portLinkControl;
    uint32_t portHardwareLPMControl;
} __attribute__((__packed__));

static_assert(sizeof(HostControllerPortRegisters) == 0x10);

struct xhci_operational_registers {
    uint32_t usbCommand;
    uint32_t usbStatus;
    uint32_t pageSize;
    uint64_t reserved1;
    uint32_t deviceNotificationControl;
    xhci_reg64 commandRingControl;
    uint64_t reserved2;
    uint64_t reserved3;
    uint64_t deviceContextBaseAddressArrayPointer;
    uint32_t config;
    uint8_t reserved4[0x400-0x3C];
    HostControllerPortRegisters portRegisters[1];
} __attribute__((__packed__));

static_assert(sizeof(xhci_operational_registers) == 0x410);

#define XHCI_INTERRUPT_MANAGEMENT_INTERRUPT_PENDING 1
#define XHCI_INTERRUPT_MANAGEMENT_INTERRUPT_ENABLE  2

#define XHCI_INTERRUPTER_DEQ_HANDLER_BUSY   8

struct xhci_interrupter_registers {
    uint32_t interrupterManagement;
    uint32_t interrupterModeration;
    uint32_t eventRingSegmentTableSize;
    uint32_t reserved;
    xhci_reg64 eventRingSegmentTableBaseAddress;
    xhci_reg64 eventRingDequeuePointer;
};
static_assert(sizeof(xhci_interrupter_registers) == 0x20);

struct xhci_runtime_registers {
    uint32_t microframeIndex; /* 00-03 */
    uint32_t reserved1; /* 04-07 */
    uint64_t reserved2; /* 08-0F */
    uint64_t reserved3; /* 10-17 */
    uint64_t reserved4; /* 18-1F */
    xhci_interrupter_registers interrupters[1];
};

struct xhci_doorbell_registers {
    uint32_t doorbells[1];
};

union xhci_legsup_cap {
    uint32_t value;
    struct {
        uint16_t cap;
        uint8_t bios_owned;
        uint8_t os_owned;
    };

    uint8_t get_os_own() {
        uint32_t os_own = value >> 24;
        return (uint8_t) os_own;
    }
    void assert_os_own() {
        os_owned = get_os_own() | 1;
    }
    bool is_owned() {
        uint32_t semas = value >> 16;
        if ((semas & 1) == 0 && (semas & 0x100) != 0) {
            return true;
        } else {
            return false;
        }
    }
};

struct xhci_ext_cap {
    uint32_t *pointer;
    union {
        uint32_t value;
        struct {
            uint8_t cap_id;
            uint8_t next_ptr;
            uint16_t cap_specific;
        } __attribute__((__packed__));
    };

    explicit xhci_ext_cap(uint32_t *pointer) : pointer(pointer), value(*pointer) {}
    xhci_ext_cap(const xhci_ext_cap &cp) noexcept : pointer(cp.pointer), value(cp.value) {}
    xhci_ext_cap(const xhci_ext_cap &&cp) noexcept : pointer(cp.pointer), value(cp.value) {}
    xhci_ext_cap & operator = (const xhci_ext_cap &cp) noexcept {
        this->pointer = cp.pointer;
        this->value = cp.value;
        return *this;
    }
    xhci_ext_cap & operator = (const xhci_ext_cap &&cp) noexcept {
        this->pointer = cp.pointer;
        this->value = cp.value;
        return *this;
    }

    std::optional<xhci_ext_cap> next() {
        if (next_ptr != 0) {
            uint32_t *ptr = pointer;
            uint16_t off = next_ptr;
            ptr += off;
            return { xhci_ext_cap(ptr) };
        } else {
            return { };
        }
    }

    xhci_legsup_cap *legsup() {
        if (cap_id == 1) {
            return (xhci_legsup_cap *) (void *) pointer;
        } else {
            return nullptr;
        }
    }
};

union xhci_hccparams1 {
    uint32_t hccparams1;
    struct {
        bool ac64 : 1;
        bool bnc : 1;
        bool csz : 1;
        bool ppc : 1;
        bool pind : 1;
        bool lhrC : 1;
        bool ltc : 1;
        bool nss : 1;
        bool pae : 1;
        bool spc : 1;
        bool sec : 1;
        bool cfc : 1;
        uint8_t maxPSASize : 4;
        uint16_t xhciExtCapPtr;
    } __attribute__((__packed__));

    explicit xhci_hccparams1(uint32_t hccparams1) noexcept : hccparams1(hccparams1) {}
    xhci_hccparams1(const xhci_hccparams1 &cp) noexcept : hccparams1(cp.hccparams1) {}
    xhci_hccparams1(const xhci_hccparams1 &&cp) noexcept : hccparams1(cp.hccparams1) {}
    xhci_hccparams1 & operator = (const xhci_hccparams1 &cp) noexcept {
        this->hccparams1 = cp.hccparams1;
        return *this;
    }
    xhci_hccparams1 & operator = (const xhci_hccparams1 &&cp) noexcept {
        this->hccparams1 = cp.hccparams1;
        return *this;
    }
};

struct xhci_capabilities {
    uint8_t caplength;
    uint8_t reserved;
    uint16_t hciversion;
    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hcsparams3;
    uint32_t hccparams1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hccparams2;

    xhci_operational_registers *opregs() {
        uint8_t *ptr = (uint8_t *) (void *) this;
        ptr += caplength;
        return (xhci_operational_registers *) (void *) ptr;
    }

    xhci_runtime_registers *runtimeregs() {
        uint8_t *ptr = (uint8_t *) (void *) this;
        ptr += rtsoff & 0xFFFFFFE0;
        return (xhci_runtime_registers *) (void *) ptr;
    }

    xhci_doorbell_registers *doorbellregs() {
        uint8_t *ptr = (uint8_t *) (void *) this;
        ptr += dboff & 0xFFFFFFFC;
        return (xhci_doorbell_registers *) (void *) ptr;
    }

    std::optional<xhci_ext_cap> extcap() {
        xhci_hccparams1 par(hccparams1);
        if (par.xhciExtCapPtr != 0) {
            uint32_t *ptr = (uint32_t *) (void *) this;
            uint32_t off = par.xhciExtCapPtr;
            ptr += off;
            return { xhci_ext_cap(ptr) };
        } else {
            return { };
        }
    }
} __attribute__((__packed__));

static_assert(sizeof(xhci_capabilities) == 0x20);

struct xhci_trb_data {
    uint64_t DataPtr;
    uint32_t TransferLength : 17;
    uint8_t TDSize : 5;
    uint16_t InterrupterTarget : 10;
} __attribute__((__packed__));

struct xhci_trb_command_completion {
    uint64_t CommandPtr;
    uint32_t CommandCompletionParameter : 24;
    uint8_t CompletionCode;
} __attribute__((__packed__));
static_assert(sizeof(xhci_trb_command_completion) == 12);

struct xhci_trb_enable_slot {
    uint8_t SlotType;
    uint8_t SlotId;
} __attribute__((__packed__));

struct xhci_trb {
    union {
        xhci_trb_data Data;
        xhci_trb_command_completion CommandCompletion;
    };
    uint16_t Command;
    xhci_trb_enable_slot EnableSlot;
} __attribute__((__packed__));
static_assert(sizeof(xhci_trb) == 16);

#define XHCI_TRB_NORMAL             (1 << 10)
#define XHCI_TRB_SETUP              (2 << 10)
#define XHCI_TRB_DATA               (3 << 10)
#define XHCI_TRB_STATUS             (4 << 10)
#define XHCI_TRB_ISOCH              (5 << 10)
#define XHCI_TRB_LINK               (6 << 10)
#define XHCI_TRB_EVENT_DATA         (7 << 10)
#define XHCI_TRB_NOOP               (8 << 10)
#define XHCI_TRB_ENABLE_SLOT        (9 << 10)
#define XHCI_TRB_DISABLE_SLOT       (10 << 10)
#define XHCI_TRB_ADDRESS_DEVICE     (11 << 10)
#define XHCI_TRB_CONFIGURE_ENDPOINT (12 << 10)

#define XHCI_TRB_CYCLE  1

template <int n> struct xhci_trb_ring {
    static_assert(n > 2);
    xhci_trb ring[n];

    xhci_trb_ring(uint64_t physAddr) : ring() {
        for (int i = 0; i < n; i++) {
            ring[i].Data.DataPtr = 0;
            ring[i].Data.TransferLength = 0;
            ring[i].Data.TDSize = 0;
            ring[i].Data.InterrupterTarget = 0;
            ring[i].Command = XHCI_TRB_NORMAL;
            ring[i].EnableSlot.SlotType = 0;
        }
        ring[n-1].Data.DataPtr = physAddr;
        ring[n-1].Command = XHCI_TRB_LINK;
    }

    constexpr int LengthIncludingLink() {
        return n;
    }
};

template <int n> struct xhci_event_ring {
    static_assert(n >= 16);
    xhci_trb ring[n];

    xhci_event_ring() : ring() {
        for (int i = 0; i < n; i++) {
            ring[i].Data.DataPtr = 0;
            ring[i].Data.TransferLength = 0;
            ring[i].Data.TDSize = 0;
            ring[i].Data.InterrupterTarget = 0;
            ring[i].Command = 0;
            ring[i].EnableSlot.SlotType = 0;
        }
    }

    constexpr int Length() {
        return n;
    }
};

struct xhci_event_segment {
    uint64_t ringSegmentBaseAddress;
    uint16_t ringSegmentSize;
    uint16_t reserved1;
    uint32_t reserved2;
};
static_assert(sizeof(xhci_event_segment) == 16);

class xhci;

class xhci_command {
    friend xhci;
private:
    hw_spinlock spinlock;
    xhci_trb *trb;
    uint64_t phys;
    uint8_t code;
    uint8_t slotID;
    bool done : 1;
public:
    xhci_command(xhci_trb *trb, uint64_t phys) : spinlock(), trb(trb), phys(phys), code(0), done(false) {}
    void SetDone() {
        std::lock_guard lock{spinlock};
        done = true;
    }
    bool IsDone() {
        critical_section cli{};
        std::lock_guard lock{spinlock};
        return done;
    }
    uint8_t CompletionCode() {
        return code;
    }
    uint8_t SlotId() {
        return slotID;
    }
};

#define XHCI_NUM_EVENT_SEGMENTS 32

struct xhci_rings {
    xhci_trb_ring<32> CommandRing;
    xhci_event_segment EventSegments[XHCI_NUM_EVENT_SEGMENTS];
    xhci_event_ring<32> PrimaryEventRing;

    xhci_rings(uint64_t physAddr) : CommandRing(physAddr), EventSegments(), PrimaryEventRing() {
        for (int i = 0; i < XHCI_NUM_EVENT_SEGMENTS; i++) {
            EventSegments[i].ringSegmentBaseAddress = 0;
            EventSegments[i].ringSegmentSize = 0;
            EventSegments[i].reserved1 = 0;
            EventSegments[i].reserved2 = 0;
        }
        EventSegments[0].ringSegmentBaseAddress = physAddr + PrimaryEventRingOffset();
        EventSegments[0].ringSegmentSize = PrimaryEventRing.Length();
    }

    constexpr uint16_t EventSegmentsOffset() {
        return sizeof(CommandRing);
    }
    constexpr uint16_t PrimaryEventRingOffset() {
        return EventSegmentsOffset() + sizeof(EventSegments);
    }
} __attribute__((__packed__));
static_assert(sizeof(xhci_rings) < 4096);

struct xhci_input_context {
    uint32_t dropContextFlags;
    uint32_t addContextFlags;
    uint32_t reserved32[5];
    uint8_t configurationValue;
    uint8_t interfaceNumber;
    uint8_t alternateSetting;
    uint8_t reserved;
    uint32_t reserved64[8];

    xhci_input_context() :
        dropContextFlags(0), addContextFlags(0), reserved32(), configurationValue(0), interfaceNumber(0),
        alternateSetting(0), reserved(0), reserved64() { }

};
static_assert(sizeof(xhci_input_context) == 64);

struct xhci_slot_context {
    uint32_t routeString : 20;
    uint8_t was_speed_now_deprecated : 4;
    uint8_t flags : 3;
    uint8_t contextEntries : 5;
    uint16_t maxExitLatency;
    uint8_t rootHubPortNumber;
    uint8_t numberOfPorts;
    uint8_t parentHubSlotId;
    uint8_t parentPortNumber;
    uint8_t ttThinkTime : 6;
    uint16_t interruperTarget : 10;
    uint8_t deviceAddress;
    uint32_t reserved : 19;
    uint8_t slotState : 5;
    uint32_t reserved2[4];

    xhci_slot_context() :
        routeString(0), was_speed_now_deprecated(0), flags(0), contextEntries(0), maxExitLatency(0), rootHubPortNumber(0),
        numberOfPorts(0), parentHubSlotId(0), parentPortNumber(0), ttThinkTime(0), interruperTarget(0), deviceAddress(0),
        reserved(0), slotState(0), reserved2() { }
} __attribute__((__packed__));
static_assert(sizeof(xhci_slot_context) == 32);

struct xhci_slot_data {
    xhci_slot_context slotContext[64];
    xhci_input_context inputContext;
    xhci_trb_ring<32> Endpoint0Ring;

    xhci_slot_data(uint64_t physaddr) : Endpoint0Ring(physaddr), inputContext(), slotContext() { }

    constexpr uint32_t SlotContextOffset() {
        return 0;
    }
    constexpr uint32_t InputContextOffset() {
        return SlotContextOffset() + sizeof(slotContext);
    }
    constexpr uint32_t Endpoint0RingOffset() {
        return InputContextOffset() + sizeof(inputContext);
    }
};

struct xhci_dcbaa {
    uint64_t phys[256];
    xhci_slot_data *contexts[256];

    xhci_dcbaa() {
        for (int i = 0; i < 256; i++) {
            phys[i] = 0;
            contexts[i] = nullptr;
        }
    }
    ~xhci_dcbaa() {
        for (int i = 0; i < 256; i++) {
            if (contexts[i] != nullptr) {
                delete contexts[i];
            }
        }
    }
};

static_assert(sizeof(xhci_dcbaa) <= 4096);

class xhci_device;

class xhci_resources {
private:
    uint8_t CommandCycle : 1;
public:
    xhci_resources() : CommandCycle(1) {
    }
    virtual ~xhci_resources() {}
    virtual uint64_t DCBAAPhys() const = 0;
    virtual xhci_dcbaa *DCBAA() const = 0;
    virtual uint64_t ScratchpadPhys() const = 0;
    virtual uint64_t CommandRingPhys() const = 0;
    virtual uint64_t PrimaryFirstEventPhys() const = 0;
    virtual uint64_t PrimaryEventSegmentsPhys() const = 0;
    virtual xhci_rings *Rings() const = 0;
    virtual std::shared_ptr<xhci_device> CreateDeviceData() const = 0;
};

class xhci_device {
public:
    virtual ~xhci_device() { }
    virtual xhci_slot_data *SlotData() const = 0;
    virtual uint64_t SlotContextPhys() const = 0;
    virtual uint64_t InputContextPhys() const = 0;
    virtual uint64_t Endpoint0RingPhys() const = 0;
};

class xhci_port_enumeration_addressing : public usb_hw_enumeration_addressing {
private:
    xhci &xhciRef;
    std::shared_ptr<xhci_device> deviceData;
    uint8_t port;
    uint8_t slot;
public:
    xhci_port_enumeration_addressing(xhci &xhciRef, uint8_t port) : xhciRef(xhciRef), port(port) {}
    std::shared_ptr<usb_hw_enumeration_ready> set_address(uint8_t addr) override;
    void disable_slot();
};

class xhci_port_enumeration : public usb_hw_enumeration {
private:
    xhci &xhciRef;
    uint8_t port;
public:
    xhci_port_enumeration(xhci &xhciRef, uint8_t port) : xhciRef(xhciRef), port(port) {}
    std::shared_ptr<usb_hw_enumeration_addressing> enumerate() override;
};

#define XHCI_TRANSFER_BUFFER_SIZE 1024

class xhci : public usb_hcd {
    friend xhci_port_enumeration;
    friend xhci_port_enumeration_addressing;
private:
    PciDeviceInformation pciDeviceInformation;
    std::unique_ptr<vmem> mapped_registers_vm;
    xhci_capabilities *capabilities;
    xhci_operational_registers *opregs;
    xhci_runtime_registers *runtimeregs;
    xhci_doorbell_registers *doorbellregs;
    std::unique_ptr<xhci_resources> resources;
    hw_spinlock xhcilock;
    raw_semaphore event_sema;
    std::thread *event_thread;
    std::vector<std::shared_ptr<xhci_command>> commands;
    xhci_trb *commandBarrier;
    uint32_t commandIndex;
    uint32_t primaryEventIndex;
    uint16_t numInterrupters;
    uint16_t eventRingSegmentTableMax;
    uint8_t numSlots;
    uint8_t numPorts;
    uint8_t commandCycle : 1;
    uint8_t primaryEventCycle : 1;
    bool stop : 1;
public:
    xhci(Bus &bus, PciDeviceInformation &deviceInformation) : usb_hcd("xhci", bus), pciDeviceInformation(deviceInformation),
    xhcilock(), event_sema(-1), event_thread(nullptr), commands(), commandBarrier(nullptr), commandIndex(0), primaryEventIndex(0),
    numInterrupters(0), eventRingSegmentTableMax(0), numSlots(0), numPorts(0), commandCycle(0), primaryEventCycle(1), stop(false) {}
    ~xhci();
    void init() override;
    uint32_t Pagesize();
    void dumpregs() override;
    int GetNumberOfPorts() override;
    uint32_t GetPortStatus(int port) override;
    void SwitchPortOff(int port) override;
    void SwitchPortOn(int port) override;
    void EnablePort(int port) override;
    void DisablePort(int port) override;
    void ResetPort(int port) override;
    usb_speed PortSpeed(int port) override;
    void ClearStatusChange(int port, uint32_t statuses) override;
    std::shared_ptr<usb_hw_enumeration> EnumeratePort(int port) override;
    std::shared_ptr<usb_endpoint> CreateControlEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) override;
    std::shared_ptr<usb_endpoint> CreateInterruptEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) override;
    size_t TransferBufferSize() override {
        return XHCI_TRANSFER_BUFFER_SIZE;
    }
    hw_spinlock &HcdSpinlock() override {
        return xhcilock;
    }
    std::tuple<uint64_t,xhci_trb *> NextCommand();
    void CommitCommand(xhci_trb *trb) {
        dumpregs();
        trb->Command = (trb->Command & 0xFFFE) | (commandCycle & 1);
        doorbellregs->doorbells[0] = 0; /* Ring the bell */
        std::stringstream str{};
        str << "Added command " << std::hex << trb->Command << " vaddr=" << ((uint64_t) trb) << "\n";
        get_klogger() << str.str().c_str();
        dumpregs();
    }
    std::shared_ptr<xhci_command> EnableSlot(uint8_t SlotType) {
        critical_section cli{};
        std::lock_guard lock{xhcilock};
        auto addr_trb = NextCommand();
        uint64_t addr = std::get<0>(addr_trb);
        xhci_trb *trb = std::get<1>(addr_trb);
        if (trb == nullptr) {
            return {};
        }
        trb->Command |= XHCI_TRB_ENABLE_SLOT;
        trb->EnableSlot.SlotType = SlotType;
        CommitCommand(trb);
        std::shared_ptr<xhci_command> ptr{new xhci_command(trb, addr)};
        commands.push_back(ptr);
        return ptr;
    }
    std::shared_ptr<xhci_command> DisableSlot(uint8_t SlotId) {
        critical_section cli{};
        std::lock_guard lock{xhcilock};
        auto addr_trb = NextCommand();
        uint64_t addr = std::get<0>(addr_trb);
        xhci_trb *trb = std::get<1>(addr_trb);
        if (trb == nullptr) {
            return {};
        }
        trb->Command |= XHCI_TRB_DISABLE_SLOT;
        trb->EnableSlot.SlotId = SlotId;
        CommitCommand(trb);
        std::shared_ptr<xhci_command> ptr{new xhci_command(trb, addr)};
        commands.push_back(ptr);
        return ptr;
    }
    std::shared_ptr<xhci_command> SetAddress(uint64_t inputContextAddr, uint8_t SlotId) {
        critical_section cli{};
        std::lock_guard lock{xhcilock};
        auto addr_trb = NextCommand();
        uint64_t addr = std::get<0>(addr_trb);
        xhci_trb *trb = std::get<1>(addr_trb);
        if (trb == nullptr) {
            return {};
        }
        trb->Command |= XHCI_TRB_ADDRESS_DEVICE;
        trb->EnableSlot.SlotId = SlotId;
        trb->Data.DataPtr = inputContextAddr;
        CommitCommand(trb);
        std::shared_ptr<xhci_command> ptr{new xhci_command(trb, addr)};
        commands.push_back(ptr);
        return ptr;
    }
private:
    bool irq();
    void HcEvent();
    void PrimaryEventRing();
    void Event(uint8_t trbtype, const xhci_trb &event);
    void CommandCompletion(const xhci_trb &event);
};

class xhci_driver : public Driver {
public:
    xhci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_XHCI_H
