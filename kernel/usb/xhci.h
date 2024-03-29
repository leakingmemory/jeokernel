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

#include "StructPool.h"
#include <concurrency/raw_semaphore.h>
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

struct xhci_supported_protocol_cap {
    uint8_t cap_id;
    uint8_t next_ptr;
    uint8_t rev_min;
    uint8_t rev_maj;
    char name[4];
    uint8_t compat_port_num;
    uint8_t compat_port_count;
    uint16_t protocol_def_field : 12;
    uint8_t protocol_speed_id_count : 4;
    uint8_t protocol_slot_type : 5;
    uint32_t reserved : 27;
    uint32_t speeds[1];

    bool is_2_0() {
        return (rev_maj == 2 && rev_min == 0);
    }
    bool is_3_0() {
        return (rev_maj == 3 && rev_min == 0);
    }
    bool is_3_1() {
        return (rev_maj == 3 && rev_min == 0x10);
    }
    bool is_3_2() {
        return (rev_maj == 3 && rev_min == 0x20);
    }
    bool is_3() {
        return rev_maj == 3;
    }
} __attribute__((__packed__));
static_assert(sizeof(xhci_supported_protocol_cap) == 20);

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
    xhci_supported_protocol_cap *supported_protocol() {
        if (cap_id == 2) {
            return (xhci_supported_protocol_cap *) (void *) pointer;
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

union xhci_hccparams2 {
    uint32_t hccparams2;
    struct {
        bool u3c : 1;
        bool cmc : 1;
        bool fsc : 1;
        bool ctc : 1;
        bool lec : 1;
        bool cic : 1;
        bool etc : 1;
        bool etc_tsc : 1;
        bool gsc : 1;
        bool vtc : 1;
        uint32_t reserved : 22;
    } __attribute__((__packed__));

    explicit xhci_hccparams2(uint32_t hccparams2) noexcept : hccparams2(hccparams2) {}
    xhci_hccparams2(const xhci_hccparams2 &cp) noexcept : hccparams2(cp.hccparams2) {}
    xhci_hccparams2(const xhci_hccparams2 &&cp) noexcept : hccparams2(cp.hccparams2) {}
    xhci_hccparams2 & operator = (const xhci_hccparams2 &cp) noexcept {
        this->hccparams2 = cp.hccparams2;
        return *this;
    }
    xhci_hccparams2 & operator = (const xhci_hccparams2 &&cp) noexcept {
        this->hccparams2 = cp.hccparams2;
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

    bool controllerIs64bit() {
        xhci_hccparams1 par(hccparams1);
        return par.ac64;
    }
    bool contextSizeIs64() {
        xhci_hccparams1 par(hccparams1);
        return par.csz;
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

struct xhci_trb_transfer_completion {
    uint64_t TransferPtr;
    uint32_t TransferLength : 24;
    uint8_t CompletionCode;
} __attribute__((__packed__));
static_assert(sizeof(xhci_trb_transfer_completion) == 12);

struct xhci_trb_set_dequeue {
    uint64_t TransferPtr;
    uint16_t Reserved;
    uint16_t StreamId;
} __attribute__((__packed__));
static_assert(sizeof(xhci_trb_set_dequeue) == 12);

struct xhci_trb_enable_slot {
    uint8_t SlotType;
    uint8_t SlotId;
} __attribute__((__packed__));

#define XHCI_SETUP_TRT_NO_DATA  0
#define XHCI_SETUP_TRT_OUT      2
#define XHCI_SETUP_TRT_IN       3
struct xhci_trb_setup {
    uint16_t TransferType;
} __attribute__((__packed__));

struct xhci_trb_transfer_event {
    uint8_t EndpointId : 5;
    uint8_t Reserved : 3;
    uint8_t SlotId;
};
static_assert(sizeof(xhci_trb_transfer_event) == 2);

struct xhci_trb_reset_endpoint {
    uint8_t EndpointId : 5;
    uint8_t Reserved : 3;
    uint8_t SlotId;
} __attribute__((__packed__));
static_assert(sizeof(xhci_trb_reset_endpoint) == 2);

#define XHCI_TRB_DIR_OUT    0
#define XHCI_TRB_DIR_IN     1
struct xhci_trb {
    union {
        xhci_trb_data Data;
        xhci_trb_command_completion CommandCompletion;
        xhci_trb_transfer_completion TransferCompletion;
        xhci_trb_set_dequeue SetDequeue;
    };
    uint16_t Command;
    union {
        xhci_trb_enable_slot EnableSlot;
        xhci_trb_setup Setup;
        xhci_trb_transfer_event TransferEvent;
        xhci_trb_reset_endpoint ResetEndpoint;
        uint16_t Flags;
    };
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
#define XHCI_TRB_EVALUATE_CONTEXT   (13 << 10)
#define XHCI_TRB_RESET_ENDPOINT     (14 << 10)
#define XHCI_TRB_STOP_ENDPOINT      (15 << 10)
#define XHCI_TRB_SET_TR_DEQUE_PTR   (16 << 10)
#define XHCI_TRB_RESET_DEVICE       (17 << 10)
#define XHCI_TRB_FORCE_EVENT_CMD    (18 << 10)
#define XHCI_TRB_NEGOTIATE_BANDW    (19 << 10)
#define XHCI_TRB_SET_LATENCY_TOLER  (20 << 10)
#define XHCI_TRB_GET_PORT_BANDWIDTH (21 << 10)
#define XHCI_TRB_FORCE_HEADER_CMD   (22 << 10)
#define XHCI_TRB_NOOP_CMD           (23 << 10)
#define XHCI_TRB_GET_EXTENDED_PROP  (24 << 10)
#define XHCI_TRB_SET_EXTENDED_PROP  (25 << 10)

#define XHCI_TRB_IMMEDIATE_DATA         (1 << 6)
#define XHCI_TRB_INTERRUPT_ON_COMPLETE  (1 << 5)
#define XHCI_TRB_INTERRUPT_ON_SHORT     (1 << 2)
#define XHCI_TRB_LINK_TOGGLE_CYCLE      2
#define XHCI_TRB_CYCLE                  1

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
        ring[n-1].Command = XHCI_TRB_LINK | XHCI_TRB_LINK_TOGGLE_CYCLE;
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

struct xhci_input_control_context {
    uint32_t dropContextFlags;
    uint32_t addContextFlags;
    uint32_t reserved32[5];
    uint8_t configurationValue;
    uint8_t interfaceNumber;
    uint8_t alternateSetting;
    uint8_t reserved;

    xhci_input_control_context() :
        dropContextFlags(0), addContextFlags(0), reserved32(), configurationValue(0), interfaceNumber(0),
        alternateSetting(0), reserved(0) { }

};
static_assert(sizeof(xhci_input_control_context) == 32);

#define XHCI_SLOT_CONTEXT_FLAG_MTT  2
#define XHCI_SLOT_CONTEXT_FLAG_HUB  4

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

#define XHCI_EP_TYPE_ISOCH_OUT      1
#define XHCI_EP_TYPE_BULK_OUT       2
#define XHCI_EP_TYPE_INTERRUPT_OUT  3
#define XHCI_EP_TYPE_CONTROL        4
#define XHCI_EP_TYPE_ISOCH_IN       5
#define XHCI_EP_TYPE_BULK_IN        6
#define XHCI_EP_TYPE_INTERRUPT_IN   7

struct xhci_endpoint_context {
    uint8_t endpointState : 3;
    uint8_t reserved1 : 5;
    uint8_t mult : 2;
    uint8_t maxPrimaryStreams : 5;
    bool linearStreamArray : 1;
    uint8_t interval;
    uint8_t maxESITPayloadHigh;
    uint8_t reserved2 : 1;
    uint8_t errorCount : 2;
    uint8_t endpointType : 3;
    uint8_t reserved3 : 1;
    bool hostInitiateDisable : 1;
    uint8_t maxBurstSize;
    uint16_t maxPacketSize;
    uint64_t trDequePointer;
    uint16_t averageTrbLength;
    uint16_t maxESITPayloadLow;
    uint32_t reserved4[3];

    xhci_endpoint_context() : endpointState(0), reserved1(0), mult(0), maxPrimaryStreams(0), linearStreamArray(false),
        interval(0), maxESITPayloadHigh(0), reserved2(0), errorCount(0), endpointType(0), reserved3(0), hostInitiateDisable(false),
        maxBurstSize(0), maxPacketSize(0), trDequePointer(0), averageTrbLength(0), maxESITPayloadLow(0), reserved4() { }
} __attribute__((__packed__));

class xhci_endpoint;

struct xhci_slot_data {
    xhci_slot_context slotContext[64];
    xhci_trb_ring<32> Endpoint0Ring;
    xhci_endpoint *Endpoints[31];
    bool stopped;

    xhci_slot_data(uint64_t physaddr) : Endpoint0Ring(physaddr + Endpoint0RingOffset()), slotContext(), stopped(false) { }

    constexpr uint32_t SlotContextOffset() {
        return 0;
    }
    constexpr uint32_t Endpoint0RingOffset() {
        return SlotContextOffset() + sizeof(slotContext);
    }

    void Stop() {
        stopped = true;
    }

    xhci_endpoint_context *EndpointContext(int index, bool contextSize64) {
        ++index;
        if (contextSize64) {
            index = index << 1;
        }
        void *pointer = (void *) &(slotContext[index]);
        return (xhci_endpoint_context *) pointer;
    }

    uint8_t GetEndpointState(int index, bool contextSize64) {
        auto *endp = EndpointContext(index, contextSize64);
        return endp->endpointState;
    }

    void SetEndpoint(int index, xhci_endpoint *endpoint) {
        Endpoints[index] = endpoint;
    }

    void UnsetEndpoint(int index, xhci_endpoint *endpoint) {
        if (Endpoints[index] == endpoint) {
            Endpoints[index] = nullptr;
        } else {
            std::stringstream str{};
            str << "Not unset event handling for endpoint " << index << "\n";
            get_klogger() << str.str().c_str();
        }
    }

    xhci_endpoint *Endpoint(int index) {
        return stopped ? nullptr : Endpoints[index];
    }

    void DumpSlotContext() {
        uint32_t *slotp = (uint32_t *) (void *) &(slotContext[0]);
        uint64_t pointer = (uint64_t) slotp;
        std::stringstream str{};
        str << "Slot dump " << std::hex << pointer << ": " << slotp[0] << " " << slotp[1] << " " << slotp[2] << " " << slotp[3] << "\n";
        get_klogger() << str.str().c_str();
    }
    void DumpEndpoint(int index, bool contextSize64) {
        uint32_t *endp = (uint32_t *) EndpointContext(index, contextSize64);
        uint64_t pointer = (uint64_t) endp;
        std::stringstream str{};
        str << "Endpoint dump " << std::hex << pointer << ": " << endp[0] << " " << endp[1] << " " << endp[2] << " " << endp[3] << " " << endp[4] << "\n";
        get_klogger() << str.str().c_str();
    }
};
static_assert(sizeof(xhci_slot_data) <= 4096);

struct xhci_input_context {
    xhci_input_control_context context[66];

    xhci_input_context() : context() {
    }

    xhci_slot_context *GetSlotContext(bool contextSize64) {
        void *pointer = (void *) &(context[contextSize64 ? 2 : 1]);
        return (xhci_slot_context *) pointer;
    }

    xhci_endpoint_context *EndpointContext(int index, bool contextSize64) {
        index += 2;
        if (contextSize64) {
            index = index << 1;
        }
        void *pointer = (void *) &(context[index]);
        return (xhci_endpoint_context *) pointer;
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

#define XHCI_BUFFER_SHORT   64

template <int n, class PhysPtr> class xhci_buffer : public usb_buffer {
private:
    std::shared_ptr<StructPoolPointer<usb_byte_buffer<n>, PhysPtr>> buffer;
public:
    xhci_buffer(std::shared_ptr<StructPoolPointer<usb_byte_buffer<n>, PhysPtr>> buffer) : buffer(buffer) {
    }
    void *Pointer() override {
        return buffer->Pointer();
    }
    uint64_t Addr() override{
        return buffer->Phys();
    }
    size_t Size() override {
        return n;
    }
};

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
    virtual std::shared_ptr<usb_buffer> Alloc(size_t size) = 0;
};

class xhci_device {
public:
    virtual ~xhci_device() { }
    virtual xhci_slot_data *SlotData() const = 0;
    virtual uint64_t SlotContextPhys() const = 0;
    virtual uint64_t Endpoint0RingPhys() const = 0;
};

class xhci_input_context_container {
public:
    virtual ~xhci_input_context_container() { }
    virtual xhci_input_context *InputContext() const = 0;
    virtual uint64_t InputContextPhys() const = 0;
};

class xhci_port_enumerated_device : public usb_hw_enumerated_device, private control_request_trait {
private:
    xhci &xhciRef;
    std::shared_ptr<xhci_device> deviceData;
    std::shared_ptr<usb_endpoint> endpoint0;
    std::shared_ptr<xhci_input_context_container> inputctx_container;
    usb_minimum_device_descriptor minDesc;
    usb_speed speed;
    uint8_t slot;
public:
    xhci_port_enumerated_device(xhci &xhciRef, std::shared_ptr<xhci_device> deviceData, std::shared_ptr<usb_endpoint> endpoint0, std::shared_ptr<xhci_input_context_container> inputctx_container, usb_minimum_device_descriptor minDesc, usb_speed speed, uint8_t slot) :
    xhciRef(xhciRef), deviceData(deviceData), endpoint0(endpoint0), inputctx_container(inputctx_container), minDesc(minDesc), speed(speed), slot(slot) {}
    ~xhci_port_enumerated_device();
    void Stop() override;
    usb_speed Speed() const override;
    uint8_t SlotId() const override;
    usb_minimum_device_descriptor MinDesc() const override;
    std::shared_ptr<usb_endpoint> Endpoint0() const override;
    bool SetHub(uint8_t numberOfPorts, bool multiTT, uint8_t ttThinkTime) override;
    bool SetConfigurationValue(uint8_t configurationValue, uint8_t interfaceNumber, uint8_t alternateSetting) override;
    std::shared_ptr<usb_endpoint> CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t endpointNum, usb_endpoint_direction dir, int pollingIntervalMs) override;
    std::shared_ptr<usb_endpoint> CreateBulkEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t endpointNum, usb_endpoint_direction dir) override;
};

class xhci_port_enumeration_addressing : public usb_hw_enumeration_addressing, private control_request_trait {
private:
    xhci &xhciRef;
    std::shared_ptr<xhci_device> deviceData;
    std::shared_ptr<xhci_input_context_container> inputctx_container;
    uint8_t rootHubPort;
    uint8_t slot;
    uint8_t addr;
public:
    xhci_port_enumeration_addressing(xhci &xhciRef, uint8_t rootHubPort) : xhciRef(xhciRef), rootHubPort(rootHubPort), slot(0), addr(0) {}
    ~xhci_port_enumeration_addressing() override = default;
    std::shared_ptr<xhci_device> DeviceData() {
        return deviceData;
    }
    std::shared_ptr<xhci_input_context_container> InputContextContainer() {
        return inputctx_container;
    }
    uint8_t Slot() {
        return slot;
    }
    xhci_slot_data *enable_slot();
    xhci_input_context *set_address(xhci_slot_data &slotData, usb_speed speed, uint32_t routeString=0, uint8_t hubSlot = 0, usb_speed hubSpeed = HIGH, uint8_t hubPort = 0);
    std::shared_ptr<usb_endpoint> configure_baseline(usb_minimum_device_descriptor &minDevDesc, xhci_input_context &inputctx);
    std::shared_ptr<usb_hw_enumerated_device> set_address(uint8_t addr) override = 0;
    uint8_t get_address() override;
    void disable_slot();
};

class xhci_root_port_enumeration_addressing : public xhci_port_enumeration_addressing {
private:
    xhci &xhciRef;
    uint8_t port;
public:
    xhci_root_port_enumeration_addressing(xhci &xhciRef, uint8_t port) : xhci_port_enumeration_addressing(xhciRef, port), xhciRef(xhciRef), port(port) {
    }
    std::shared_ptr<usb_hw_enumerated_device> set_address(uint8_t addr) override;
};

class xhci_hub_port_enumeration_addressing : public xhci_port_enumeration_addressing {
private:
    xhci &xhciRef;
    usb_speed speed;
    usb_speed hubSpeed;
    uint32_t routeString;
    uint8_t hubSlot;
    uint8_t hubPort;
public:
    xhci_hub_port_enumeration_addressing(xhci &xhciRef, const std::vector<uint8_t> &portRouting, usb_speed speed, uint8_t hubSlot, usb_speed hubSpeed);
    std::shared_ptr<usb_hw_enumerated_device> set_address(uint8_t addr) override;
};

class xhci_port_enumeration : public usb_hw_enumeration {
private:
    xhci &xhciRef;
    uint8_t port;
public:
    xhci_port_enumeration(xhci &xhciRef, uint8_t port) : xhciRef(xhciRef), port(port) {}
    std::shared_ptr<usb_hw_enumeration_addressing> enumerate() override;
};

class xhci_endpoint;

#define XHCI_TRANSFER_BUFFER_SIZE 1024

class xhci : public usb_hcd {
    friend xhci_port_enumerated_device;
    friend xhci_port_enumeration;
    friend xhci_port_enumeration_addressing;
    friend xhci_endpoint;
private:
    PciDeviceInformation pciDeviceInformation;
    std::unique_ptr<vmem> mapped_registers_vm;
    xhci_capabilities *capabilities;
    xhci_operational_registers *opregs;
    xhci_runtime_registers *runtimeregs;
    xhci_doorbell_registers *doorbellregs;
    std::unique_ptr<xhci_resources> resources;
    std::vector<xhci_supported_protocol_cap *> supported_protocol_list;
    hw_spinlock xhcilock;
    raw_semaphore event_sema;
    std::thread *event_thread;
    std::thread *watchdog_thread;
    std::vector<std::shared_ptr<xhci_command>> commands;
    std::vector<xhci_trb> events;
    xhci_trb *commandBarrier;
    uint32_t commandIndex;
    uint32_t primaryEventIndex;
    uint16_t u2Exit;
    uint16_t numInterrupters;
    uint16_t eventRingSegmentTableMax;
    uint8_t u1Exit;
    uint8_t numSlots;
    uint8_t numPorts;
    uint8_t commandCycle : 1;
    uint8_t primaryEventCycle : 1;
    bool stop : 1;
    bool controller64bit : 1;
    bool contextSize64 : 1;
    bool irqWatchdog : 1;
public:
    xhci(Bus &bus, PciDeviceInformation &deviceInformation) : usb_hcd("xhci", bus), pciDeviceInformation(deviceInformation),
    supported_protocol_list(), xhcilock(), event_sema(-1), event_thread(nullptr), watchdog_thread(nullptr), commands(),
    events(), commandBarrier(nullptr), commandIndex(0), primaryEventIndex(0), u2Exit(0), numInterrupters(0),
    eventRingSegmentTableMax(0), u1Exit(0), numSlots(0), numPorts(0), commandCycle(0), primaryEventCycle(1), stop(false),
    controller64bit(false), contextSize64(false), irqWatchdog(false) {}
    ~xhci();
    void init() override;
    uint32_t Pagesize();
    void dumpregs() override;
    int GetNumberOfPorts() override;
    usb_speed HubSpeed() override {
        return SUPERPLUS;
    }
private:
    void Watchdog();
public:
    uint32_t GetPortStatus(int port) override;
    void SwitchPortOff(int port) override;
    void SwitchPortOn(int port) override;
    void EnablePort(int port) override;
    void DisablePort(int port) override;
    void ResetPort(int port) override;
    xhci_supported_protocol_cap *SupportedProtocolsForPort(int port);
    usb_speed PortSpeed(int port) override;
    void ClearStatusChange(int port, uint32_t statuses) override;
    std::shared_ptr<usb_hw_enumeration> EnumeratePort(int port) override;
    std::shared_ptr<usb_hw_enumeration_addressing> EnumerateHubPort(const std::vector<uint8_t> &portRouting, usb_speed speed, usb_speed hubSpeed, uint8_t hubSlot) override;
    std::shared_ptr<usb_endpoint> CreateControlEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) override;
    std::shared_ptr<usb_endpoint> CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) override;
    std::shared_ptr<usb_endpoint> CreateBulkEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) override;
    size_t TransferBufferSize() override {
        return XHCI_TRANSFER_BUFFER_SIZE;
    }
    std::shared_ptr<usb_buffer> Alloc(size_t size);
    hw_spinlock &HcdSpinlock() override {
        return xhcilock;
    }
    std::tuple<uint64_t,xhci_trb *> NextCommand();
    void CommitCommand(xhci_trb *trb) {
        trb->Command = (trb->Command & 0xFFFE) | (commandCycle & 1);
        doorbellregs->doorbells[0] = 0; /* Ring the bell */
        std::stringstream str{};
    }
    std::shared_ptr<xhci_command> EnableSlot(uint8_t SlotType) {
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
    std::shared_ptr<xhci_command> ConfigureEndpoint(uint64_t inputContextAddr, uint8_t SlotId) {
        std::lock_guard lock{xhcilock};
        auto addr_trb = NextCommand();
        uint64_t addr = std::get<0>(addr_trb);
        xhci_trb *trb = std::get<1>(addr_trb);
        if (trb == nullptr) {
            return {};
        }
        trb->Command |= XHCI_TRB_CONFIGURE_ENDPOINT;
        trb->EnableSlot.SlotId = SlotId;
        trb->Data.DataPtr = inputContextAddr;
        CommitCommand(trb);
        std::shared_ptr<xhci_command> ptr{new xhci_command(trb, addr)};
        commands.push_back(ptr);
        return ptr;
    }
    std::shared_ptr<xhci_command> EvaluateContext(uint64_t inputContextAddr, uint8_t SlotId) {
        std::lock_guard lock{xhcilock};
        auto addr_trb = NextCommand();
        uint64_t addr = std::get<0>(addr_trb);
        xhci_trb *trb = std::get<1>(addr_trb);
        if (trb == nullptr) {
            return {};
        }
        trb->Command |= XHCI_TRB_EVALUATE_CONTEXT;
        trb->EnableSlot.SlotId = SlotId;
        trb->Data.DataPtr = inputContextAddr;
        CommitCommand(trb);
        std::shared_ptr<xhci_command> ptr{new xhci_command(trb, addr)};
        commands.push_back(ptr);
        return ptr;
    }
    std::shared_ptr<xhci_command> ResetEndpoint(uint8_t slot, uint8_t endpoint_id) {
        std::lock_guard lock{xhcilock};
        auto addr_trb = NextCommand();
        uint64_t addr = std::get<0>(addr_trb);
        xhci_trb *trb = std::get<1>(addr_trb);
        if (trb == nullptr) {
            return {};
        }
        trb->Command |= XHCI_TRB_RESET_ENDPOINT;
        trb->ResetEndpoint.SlotId = slot;
        trb->ResetEndpoint.EndpointId = endpoint_id;
        CommitCommand(trb);
        std::shared_ptr<xhci_command> ptr{new xhci_command(trb, addr)};
        commands.push_back(ptr);
        return ptr;
    }
    std::shared_ptr<xhci_command> SetDequeuePtr(uint8_t slot, uint8_t endpoint_id, uint8_t sct, uint16_t streamId, uint64_t dequeue) {
        std::lock_guard lock{xhcilock};
        auto addr_trb = NextCommand();
        uint64_t addr = std::get<0>(addr_trb);
        xhci_trb *trb = std::get<1>(addr_trb);
        if (trb == nullptr) {
            return {};
        }
        trb->Command |= XHCI_TRB_SET_TR_DEQUE_PTR;
        trb->SetDequeue.TransferPtr = dequeue & 0xFFFFFFFFFFFFFFF1;
        trb->SetDequeue.TransferPtr |= (sct << 1) & 0x0E;
        trb->SetDequeue.StreamId = streamId;
        trb->ResetEndpoint.SlotId = slot;
        trb->ResetEndpoint.EndpointId = endpoint_id;
        CommitCommand(trb);
        std::shared_ptr<xhci_command> ptr{new xhci_command(trb, addr)};
        commands.push_back(ptr);
        return ptr;
    }
    std::shared_ptr<xhci_command> StopEndpoint(uint8_t slot, uint8_t endpoint_id) {
        std::lock_guard lock{xhcilock};
        auto addr_trb = NextCommand();
        uint64_t addr = std::get<0>(addr_trb);
        xhci_trb *trb = std::get<1>(addr_trb);
        if (trb == nullptr) {
            return {};
        }
        trb->Command |= XHCI_TRB_STOP_ENDPOINT;
        trb->ResetEndpoint.SlotId = slot;
        trb->ResetEndpoint.EndpointId = endpoint_id;
        CommitCommand(trb);
        std::shared_ptr<xhci_command> ptr{new xhci_command(trb, addr)};
        commands.push_back(ptr);
        return ptr;
    }
private:
    bool irq();
    void HcEvent();
    void PrimaryEventRing();
    void PrimaryEventRingAsync();
    void Event(uint8_t trbtype, const xhci_trb &event);
    void CommandCompletion(const xhci_trb &event);
    void TransferEvent(const xhci_trb &event);
};

class xhci_driver : public Driver {
public:
    xhci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_XHCI_H
