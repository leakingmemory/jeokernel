//
// Created by sigsegv on 6/15/21.
//

#include <acpi/acpica.h>
extern "C" {
    #include <acpi.h>
    #include <acpiosxf.h>
    #include <acpixf.h>
    #include <acobject.h>
    #include <acglobal.h>
}
#include <klogger.h>
#include <stdio.h>
#include <stdarg.h>
#include <sstream>

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
    get_acpica().read_pci_reg(PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, Reg, *Value, Width);
}

ACPI_STATUS
AcpiOsWritePciConfiguration (
        ACPI_PCI_ID             *PciId,
        UINT32                  Reg,
        UINT64                  Value,
        UINT32                  Width) {
    get_acpica().write_pci_reg(PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, Reg, Value, Width);
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