//
// Created by sigsegv on 6/15/21.
//

extern "C" {
    #include <acpi.h>
    #include <acpiosxf.h>
    #include <acpixf.h>
    #include <acobject.h>
    #include <acglobal.h>
}
#include <acpi/acpica.h>
#include <klogger.h>
#include <stdio.h>
#include <stdarg.h>
#include <sstream>
#include <pagealloc.h>

//#define INIT_TABLES_WITH_INTS_DISABLED
#define USE_ACPI_FIND_ROOT
//#define WARN_NULL_LOCK

acpica_lib::acpica_lib() {
}

acpica_lib::~acpica_lib() {
    AcpiTerminate();
}

void acpica_lib::terminate() {

}

void acpica_lib::bootstrap() {
    ACPI_STATUS Status;

    /* Initialize the ACPICA subsystem */
    pmemcounts();
    Status = AcpiInitializeSubsystem ();
    if (ACPI_FAILURE (Status)) {
        {
            std::stringstream ss{};
            ss << "Error code " << Status << " from acpica : " << AcpiFormatException(Status) << "\n";
            get_klogger() << ss.str().c_str();
        }
        wild_panic("Failed to initialize acpica subsystem");
    }

    {
        std::stringstream ss{};
        ss << "ACPICA initialized subsystem, tables pending\n";
        get_klogger() << ss.str().c_str();
    }

    {
#ifdef INIT_TABLES_WITH_INTS_DISABLED
        critical_section cli{};
#endif
        Status = AcpiInitializeTables((ACPI_TABLE_DESC *) NULL, 32, FALSE);
        if (ACPI_FAILURE(Status)) {
            {
                std::stringstream ss{};
                ss << "Error code " << Status << " from acpica : " << AcpiFormatException(Status) << "\n";
                get_klogger() << ss.str().c_str();
            }
            wild_panic("Failed to initialize acpica tables");
        }
    }

    Status = AcpiLoadTables();
    if (ACPI_FAILURE(Status)) {
        {
            std::stringstream ss{};
            ss << "Error code " << Status << " from acpica : " << AcpiFormatException(Status) << "\n";
            get_klogger() << ss.str().c_str();
        }
        wild_panic("Failed to load acpica tables");
    }

    {
        std::stringstream ss{};
        ss << "ACPICA initialized with subsystem and tables, enabling\n";
        get_klogger() << ss.str().c_str();
    }

    Status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(Status)) {
        {
            std::stringstream ss{};
            ss << "Error code " << Status << " from acpica : " << AcpiFormatException(Status) << "\n";
            get_klogger() << ss.str().c_str();
        }
        wild_panic("Failed to enable acpica subsystem");
    }

    Status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(Status)) {
        {
            std::stringstream ss{};
            ss << "Error code " << Status << " from acpica : " << AcpiFormatException(Status) << "\n";
            get_klogger() << ss.str().c_str();
        }
        wild_panic("Failed to fully initialize acpica objects");
    }

    {
        std::stringstream ss{};
        ss << "ACPICA enabled subsystem and objects - completed initialization process\n";
        get_klogger() << ss.str().c_str();
    }

    {
        ACPI_OBJECT picmode_arg;
        picmode_arg.Type = ACPI_TYPE_INTEGER;
        picmode_arg.Integer.Value = 1; /* IOAPIC */
        ACPI_OBJECT_LIST args;
        args.Count = 1;
        args.Pointer = &picmode_arg;
        ACPI_BUFFER return_value;
        return_value.Pointer = NULL;
        return_value.Length = ACPI_ALLOCATE_BUFFER;
        ACPI_STATUS picstatus = AcpiEvaluateObject(NULL, "\\_PIC", &args, &return_value);
        if (picstatus == AE_OK || picstatus == AE_NOT_FOUND) {
            get_klogger() << "ACPI set to IOAPIC mode\n";
            if (return_value.Pointer != NULL) {
                ACPI_FREE(return_value.Pointer);
            }
        } else {
            get_klogger() << "ERROR: Failed to set ACPI to IOAPIC mode\n";
        }
    }
    AcpiOsSleep(5000);
}

bool acpica_lib::evaluate_integer(void *acpi_handle, const char *method, uint64_t &value) {
    ACPI_OBJECT obj;
    ACPI_BUFFER buf{
        .Length = sizeof(obj),
        .Pointer = &obj
    };
    ACPI_STATUS Status = AcpiEvaluateObject((ACPI_HANDLE) acpi_handle, (char *) method, (ACPI_OBJECT_LIST *) NULL, &buf);
    if (Status == AE_OK) {
        if (obj.Type == ACPI_TYPE_INTEGER) {
            value = obj.Integer.Value;
            return true;
        }
        return false;
    } else {
        return false;
    }
}

typedef std::function<ACPI_STATUS (ACPI_HANDLE, UINT32, void **)> dev_walk_func;
typedef std::function<ACPI_STATUS (ACPI_RESOURCE *)> res_walk_func;
typedef std::function<ACPI_STATUS (ACPI_HANDLE, UINT32, void **)> ns_walk_func;

extern "C" {
    static ACPI_STATUS wf_dev_walk(
            ACPI_HANDLE Object,
            UINT32 NestingLevel,
            void *Context,
            void **ReturnValue) {
        dev_walk_func *func = (dev_walk_func *) Context;
        return (*func)(Object, NestingLevel, ReturnValue);
    }
    static ACPI_STATUS wf_res_walk(
            ACPI_RESOURCE *resource,
            void *Context
            ) {
        res_walk_func *func = (res_walk_func *) Context;
        return (*func)(resource);
    }
    static ACPI_STATUS wf_ns_walk(
            ACPI_HANDLE object,
            UINT32 NestingLevel,
            void *Context,
            void **ReturnValue
            ) {
        ns_walk_func *func = (ns_walk_func *) Context;
        return (*func)(object, NestingLevel, ReturnValue);
    }
}

bool acpica_lib::find_resources(void *handle, std::function<void (ACPI_RESOURCE *)> wfunc) {
    res_walk_func func = [wfunc] (ACPI_RESOURCE *resource) mutable -> ACPI_STATUS {
        wfunc(resource);
        return AE_OK;
    };
    ACPI_STATUS Status = AcpiWalkResources((ACPI_HANDLE) handle, METHOD_NAME__CRS, (ACPI_WALK_RESOURCE_CALLBACK) &wf_res_walk, (void *) &func);
    return Status == AE_OK;
}

bool acpica_lib::find_pci_bridges(std::function<void (void *handle, ACPI_DEVICE_INFO *dev_info)> wfunc) {
    void *retv;
    dev_walk_func func = [wfunc] (ACPI_HANDLE handle, uint32_t depth, void **retvp) mutable {
        ACPI_DEVICE_INFO *dev_info;
        ACPI_STATUS Status = AcpiGetObjectInfo(handle, &dev_info);
        if (Status == AE_OK) {
            if (dev_info->HardwareId.Length > 0 && strncmp(dev_info->HardwareId.String, "PNP0A", 5) == 0) {
                wfunc((void *) handle, dev_info);
            }
        }
        return AE_OK;
    };
    ACPI_STATUS Status = AcpiGetDevices((char *) NULL, wf_dev_walk, (void *) &func, &retv);
    return Status == AE_OK;
}

bool acpica_lib::determine_pci_id(ACPI_PCI_ID &pciId, void *vhandle) {
    ACPI_HANDLE handle = (ACPI_HANDLE) vhandle;
    ACPI_DEVICE_INFO *dev_info;
    if (AcpiGetObjectInfo(handle, &dev_info) == AE_OK) {
        return determine_pci_id(pciId, dev_info, vhandle);
    } else {
        return false;
    }
}

bool acpica_lib::determine_pci_id(ACPI_PCI_ID &pciId, const ACPI_DEVICE_INFO *dev_info, void *vhandle) {
    ACPI_HANDLE handle = (ACPI_HANDLE) vhandle;
    pciId.Device = 0;
    pciId.Function = 0;
    pciId.Bus = 0;
    pciId.Segment = 0;
    if (dev_info->Flags & ACPI_PCI_ROOT_BRIDGE) {
        uint64_t bnn;
        if (evaluate_integer(handle, "_BNN", bnn)) {
            pciId.Bus = (uint16_t) (bnn & 0xFFFF);
        }
        uint64_t seg;
        if (evaluate_integer(handle, "_SEG", seg)) {
            pciId.Segment = (uint16_t) (seg & 0xFFFF);
        }
        return true;
    }
    pciId.Device = (uint16_t) ((dev_info->Address >> 16) & 0xFFFF);
    pciId.Function = (uint16_t) (dev_info->Address & 0xFFFF);
    ACPI_HANDLE parent_h;
    if (AcpiGetParent(handle, &parent_h) == AE_OK) {
        ACPI_DEVICE_INFO *p_dev_info;
        if (AcpiGetObjectInfo(handle, &p_dev_info) != AE_OK) {
            //get_klogger() << "Parent has no dev info\n";
            return false;
        }
        ACPI_PCI_ID ParentPciID;
        if (determine_pci_id(ParentPciID, p_dev_info, parent_h)) {
            uint64_t pcival{0};
            if (AcpiOsReadPciConfiguration(&ParentPciID, 0x0c, &pcival, 32) != AE_OK) {
                //get_klogger() << "Bridge type read error\n";
                return false;
            }
            uint8_t bridge_type = (uint8_t) ((pcival >> 8) & 0x7F);
            if (bridge_type == 0 && (p_dev_info->Flags & ACPI_PCI_ROOT_BRIDGE)) {
                pciId.Segment = ParentPciID.Segment;
                pciId.Bus = ParentPciID.Bus;
                return true;
            }
            if (bridge_type != 1 && bridge_type != 2) {
                //get_klogger() << "Bridge type error " << bridge_type << "\n";
                return false;
            }
            if (AcpiOsReadPciConfiguration(&ParentPciID, 0x19, &pcival, 8) != AE_OK) {
                //get_klogger() << "Bridge bus number cfg error\n";
                return false;
            }
            pciId.Segment = ParentPciID.Segment;
            pciId.Bus = (uint8_t) (pcival & 0xFF);
            return true;
        } else {
            //get_klogger() << "Not pci bridge\n";
        }
    }
    return false;
}

acpibuffer acpica_lib::get_irq_routing_table(void *handle) {
    ACPI_BUFFER buf{
        .Pointer = 0,
        .Length = ACPI_ALLOCATE_BUFFER
    };
    if (AcpiGetIrqRoutingTable((ACPI_HANDLE) handle, &buf) == AE_OK) {
        acpibuffer abuf{buf.Pointer, buf.Length};
        ACPI_FREE(buf.Pointer);
        return abuf;
    }
    return acpibuffer();
}

std::vector<PciIRQRouting> acpica_lib::get_irq_routing(void *handle) {
    std::vector<PciIRQRouting> rtvec{};
    {
        acpibuffer buf = get_irq_routing_table(handle);
        if (buf.length > 0) {
            ACPI_PCI_ROUTING_TABLE *a_rt = (ACPI_PCI_ROUTING_TABLE *) buf.ptr;
            while (a_rt->Length > 0) {
                PciIRQRouting irqr{
                    .Address = a_rt->Address,
                    .Pin = a_rt->Pin,
                    .SourceIndex = a_rt->SourceIndex,
                    .Source{&(a_rt->Source[0])}
                };
                rtvec.push_back(irqr);
                a_rt = (ACPI_PCI_ROUTING_TABLE *) (((uint8_t *) a_rt) + a_rt->Length);
            }
        }
    }
    return rtvec;
}

bool acpica_lib::walk_namespace(std::function<void(std::string, uint32_t, void *)> wfunc) {
    return walk_namespace((void *) ACPI_ROOT_OBJECT, wfunc);
}

bool acpica_lib::walk_namespace(void *handle, std::function<void(std::string, uint32_t, void *)> wfunc) {
    ns_walk_func descending = [&wfunc] (ACPI_HANDLE h, UINT32 depth, void **rv) -> ACPI_STATUS {
        acpi_buffer buf{
            .Pointer = NULL,
            .Length = ACPI_ALLOCATE_BUFFER
        };
        if (AcpiGetName(h, ACPI_FULL_PATHNAME, &buf) == AE_OK) {
            std::string pathname{(char *) buf.Pointer};
            ACPI_FREE(buf.Pointer);

            ACPI_OBJECT_TYPE objectType{0};
            AcpiGetType(h, &objectType);

            wfunc(pathname, objectType, h);
        }
        return AE_OK;
    };
    void *rv;
    auto status = AcpiWalkNamespace(ACPI_TYPE_ANY, (ACPI_HANDLE) handle, 32, wf_ns_walk, (ACPI_WALK_CALLBACK) NULL, (void *) &descending, &rv);
    if (status == AE_OK) {
        return true;
    } else {
        return false;
    }
}

std::optional<IRQLink> acpica_lib::get_extended_irq(void *handle) {
    std::optional<IRQLink> opt{};
    find_resources(handle, [&opt] (ACPI_RESOURCE *resource) {
       if (resource->Type == ACPI_RESOURCE_TYPE_EXTENDED_IRQ) {
           const auto &ei = resource->Data.ExtendedIrq;
           IRQLink link{
               .ProducerConsumer = ei.ProducerConsumer,
               .Triggering = ei.Triggering,
               .Polarity = ei.Polarity,
               .Shareable = ei.Shareable,
               .WakeCapable = ei.WakeCapable,
               .ResourceSource = ei.ResourceSource.StringPtr != nullptr && ei.ResourceSource.StringLength > 0 ? std::string(ei.ResourceSource.StringPtr) : std::string(),
               .Interrupts = std::vector<uint8_t>()
           };
           link.Interrupts.reserve(ei.InterruptCount);
           for (int i = 0; i < ei.InterruptCount; i++) {
               link.Interrupts.push_back(ei.Interrupts[i]);
           }
           opt = link;
       }
    });
    return opt;
}

static acpica_lib *acpica_lib_singleton = nullptr;

void init_acpica(uint64_t root_table_addr) {
    acpica_lib_singleton = new acpica_lib_impl(root_table_addr);
    acpica_lib_singleton->bootstrap();
}

acpica_lib *get_acpica_ptr() {
    return acpica_lib_singleton;
}

acpica_lib &get_acpica() {
    return *acpica_lib_singleton;
}

extern "C" {

ACPI_STATUS AcpiOsTerminate(void) {
    acpica_lib *lib{get_acpica_ptr()};
    if (lib != nullptr) {
        lib->terminate();
    }
    return AE_OK;
}

void *
AcpiOsAllocateZeroed(
        ACPI_SIZE Size) {
    return get_acpica().allocate_zeroed(Size);
}

void *
AcpiOsAllocate (
        ACPI_SIZE               Size) {
    return get_acpica().allocate(Size);
}

void
AcpiOsFree(
        void *Memory) {
    get_acpica().free(Memory);
}

ACPI_STATUS
AcpiOsCreateLock(
        ACPI_SPINLOCK*OutHandle) {
    *OutHandle = (ACPI_SPINLOCK) get_acpica().create_lock();
    return AE_OK;
}

void
AcpiOsDeleteLock(
        ACPI_SPINLOCK Handle) {
    get_acpica().destroy_lock((void *) Handle);
}

ACPI_CPU_FLAGS
AcpiOsAcquireLock(
        ACPI_SPINLOCK Handle) {
    if (Handle == NULL) {
#ifdef WARN_NULL_LOCK
        get_klogger() << "Warning: ACPICA tries to lock NULL\n";
#endif
        return 0;
    }
    return (uint64_t) get_acpica().acquire_lock((void *) Handle);
}

void
AcpiOsReleaseLock(
        ACPI_SPINLOCK Handle,
        ACPI_CPU_FLAGS Flags) {
    if (Flags == 0) {
        return;
    }
    get_acpica().release_lock((void *) Handle, (void *) Flags);
}


ACPI_STATUS
AcpiOsCreateSemaphore(
        UINT32 MaxUnits,
        UINT32 InitialUnits,
        ACPI_SEMAPHORE*OutHandle) {
    void *sema = get_acpica().create_semaphore(MaxUnits, InitialUnits);
    if (sema != nullptr) {
        *OutHandle = (ACPI_SEMAPHORE) sema;
        return AE_OK;
    } else {
        return AE_BAD_VALUE;
    }
}

ACPI_STATUS
AcpiOsDeleteSemaphore(
        ACPI_SEMAPHORE Handle) {
    get_acpica().destroy_semaphore((void *) Handle);
    return AE_OK;
}

ACPI_THREAD_ID
AcpiOsGetThreadId(
        void) {
    return get_acpica().get_thread_id();
}

ACPI_STATUS
AcpiOsWaitSemaphore(
        ACPI_SEMAPHORE Handle,
        UINT32 Units,
        UINT16 Timeout) {
    get_acpica().wait_semaphore((void *) Handle, Units);
    return AE_OK;
}

ACPI_STATUS
AcpiOsSignalSemaphore(
        ACPI_SEMAPHORE Handle,
        UINT32 Units) {
    get_acpica().signal_semaphore((void *) Handle, Units);
    return AE_OK;
}

ACPI_STATUS
AcpiOsExecute(
        ACPI_EXECUTE_TYPE Type,
        ACPI_OSD_EXEC_CALLBACK Function,
        void *Context) {
    AcpiExecuteType type{};
    switch (Type) {
        case OSL_GLOBAL_LOCK_HANDLER: {
            type = AcpiExecuteType::GLOBAL_LOCK_HANDLER;
        }
            break;
        case OSL_NOTIFY_HANDLER: {
            type = AcpiExecuteType::NOTIFY_HANDLER;
        }
            break;
        case OSL_GPE_HANDLER: {
            type = AcpiExecuteType::GPE_HANDLER;
        }
            break;
        case OSL_DEBUGGER_MAIN_THREAD: {
            type = AcpiExecuteType::DEBUGGER_MAIN_THREAD;
        }
            break;
        case OSL_DEBUGGER_EXEC_THREAD: {
            type = AcpiExecuteType::DEBUGGER_EXEC_THREAD;
        }
            break;
        case OSL_EC_POLL_HANDLER: {
            type = AcpiExecuteType::EC_POLL_HANDLER;
        }
            break;
        case OSL_EC_BURST_HANDLER: {
            type = AcpiExecuteType::EC_BURST_HANDLER;
        }
            break;
    }
    get_acpica().execute(type, [Function, Context]() {
        Function(Context);
    });
    return AE_OK;
}

void
AcpiOsStall(
        UINT32 Microseconds) {
    uint64_t nanos = Microseconds;
    nanos = nanos * 1000;
    get_acpica().stall_nano(nanos);
}

void
AcpiOsSleep(
        UINT64 Milliseconds) {
    uint64_t nanos = Milliseconds;
    nanos = nanos * 1000000;
    get_acpica().sleep_nano(nanos);
}

void ACPI_INTERNAL_VAR_XFACE
AcpiOsPrintf(
        const char *Format,
        ...) {
    va_list args;
    va_start(args, Format);
    AcpiOsVprintf(Format, args);
    va_end(args);
}

void
AcpiOsVprintf(
        const char *Format,
        va_list Args) {
    vprintf(Format, Args);
}

ACPI_STATUS
AcpiOsReadPort (
        ACPI_IO_ADDRESS         Address,
        UINT32                  *Value,
        UINT32                  Width) {
    if (Address < 0x100000000) {
        if (get_acpica().read_port(Address, *Value, Width)) {
            return AE_OK;
        } else {
            return AE_BAD_VALUE;
        }
    } else {
        return AE_BAD_ADDRESS;
    }
}

ACPI_STATUS
AcpiOsWritePort (
        ACPI_IO_ADDRESS         Address,
        UINT32                  Value,
        UINT32                  Width) {
    if (Address < 0x100000000) {
        if (get_acpica().write_port(Address, Value, Width)) {
            return AE_OK;
        } else {
            return AE_BAD_VALUE;
        }
    } else {
        return AE_BAD_ADDRESS;
    }
}

ACPI_STATUS
AcpiOsReadMemory (
        ACPI_PHYSICAL_ADDRESS   Address,
        UINT64                  *Value,
        UINT32                  Width) {
    if (get_acpica().read_memory(Address, *Value, Width)) {
        return AE_OK;
    } else {
        return AE_BAD_VALUE;
    }
}

ACPI_STATUS
AcpiOsWriteMemory (
        ACPI_PHYSICAL_ADDRESS   Address,
        UINT64                  Value,
        UINT32                  Width) {
    if (get_acpica().write_memory(Address, Value, Width)) {
        return AE_OK;
    } else {
        return AE_BAD_VALUE;
    }
}

ACPI_STATUS
AcpiOsInstallInterruptHandler (
        UINT32                  InterruptNumber,
        ACPI_OSD_HANDLER        ServiceRoutine,
        void                    *Context) {
    get_acpica().install_int_handler(InterruptNumber, ServiceRoutine, Context);
    return AE_OK;
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler (
        UINT32                  InterruptNumber,
        ACPI_OSD_HANDLER        ServiceRoutine) {
    get_acpica().remove_int_handler(InterruptNumber, ServiceRoutine);
    return AE_OK;
}

void
AcpiOsWaitEventsComplete() {
    get_acpica().wait_events_complete();
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue) {
    *NewValue = nullptr;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable) {
    *NewTable = nullptr;
    return AE_OK;
}

ACPI_STATUS
AcpiOsPhysicalTableOverride (
        ACPI_TABLE_HEADER       *ExistingTable,
        ACPI_PHYSICAL_ADDRESS   *NewAddress,
        UINT32                  *NewTableLength) {
    *NewAddress = 0;
    *NewTableLength = 0;
    return AE_OK;
}

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length) {
    return get_acpica().map_memory(PhysicalAddress, Length);
}

void AcpiOsUnmapMemory(void *where, ACPI_SIZE length) {
    get_acpica().unmap_memory(where, length);
}

ACPI_STATUS AcpiOsInitialize() {
    return AE_OK;
}

ACPI_STATUS
AcpiOsReadPciConfiguration (
        ACPI_PCI_ID             *PciId,
        UINT32                  Reg,
        UINT64                  *Value,
        UINT32                  Width) {
    bool success = get_acpica().read_pci_reg(PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, Reg, *Value, Width);
    if (success) {
        return AE_OK;
    } else {
        return AE_BAD_VALUE;
    }
}

ACPI_STATUS
AcpiOsWritePciConfiguration (
        ACPI_PCI_ID             *PciId,
        UINT32                  Reg,
        UINT64                  Value,
        UINT32                  Width) {
    bool success = get_acpica().write_pci_reg(PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, Reg, Value, Width);
    if (success) {
        return AE_OK;
    } else {
        return AE_BAD_ADDRESS;
    }
}

void
AcpiOsRedirectOutput (
        void                    *Destination) {
}

UINT64
AcpiOsGetTimer (
        void) {
    return get_acpica().get_timer_100ns();
}

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer (
        void) {
#ifdef USE_ACPI_FIND_ROOT
    ACPI_PHYSICAL_ADDRESS root_p;
    get_klogger() << "Looking for root pointer\n";
    ACPI_STATUS status = AcpiFindRootPointer(&root_p);
    if (status != AE_OK) {
        get_klogger() << "Failed to find root pointer\n";
        root_p = get_acpica().get_root_addr();
    }
    {
        std::stringstream ss{};
        ss << "Found root pointer " << std::hex << root_p << "\n";
        get_klogger() << ss.str().c_str();
    }
    return root_p;
#else
    return get_acpica().get_root_addr();
#endif
}

ACPI_STATUS
AcpiPurgeCachedObjects (
        void) {
    return AE_OK;
}

ACPI_STATUS
AcpiOsSignal (
        UINT32                  Function,
        void                    *Info) {
    return AE_OK;
}

}