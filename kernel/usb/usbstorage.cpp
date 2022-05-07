//
// Created by sigsegv on 2/26/22.
//

#include "usb_port_connection.h"
#include "usbstorage.h"
#include "usb_transfer.h"
#include <sstream>
#include "ustorage_structs.h"
#include "../scsi/scsidev.h"

//#define USBSTORAGE_COMMAND_DEBUG
//#define USBSTORAGE_DEBUG_TRANSFERS
//#define USBSTORAGE_EXPECT_SHORT_READ
//#define USBSTORAGE_DEBUG_SHORT_READ

#define IFACE_PROTO_BULK_ONLY 0x50

#define USBSTORAGE_SUBCLASS_SCSI    6

struct CommandBlockWrapper {
    uint32_t dCBWSignature;
    uint32_t dCBWTag;
    uint32_t dCBWDataTransferLength;
    uint8_t bmCBWFlags;
    uint8_t LUN;
    uint8_t CBLength;
    uint8_t CommandBlock[16];

    CommandBlockWrapper(uint32_t tag, uint32_t dataTransferLength, uint8_t flags, uint8_t lun, uint8_t commandBlockLength, const void *commandBlock)
    : dCBWSignature(0x43425355), dCBWTag(tag), dCBWDataTransferLength(dataTransferLength), bmCBWFlags(flags), LUN(lun), CBLength(commandBlockLength),
    CommandBlock() {
        if (commandBlockLength <= 16) {
            memcpy(&(CommandBlock[0]), commandBlock, commandBlockLength);
        }
    }
    template <class T> CommandBlockWrapper(uint32_t tag, uint32_t dataTransferLength, uint8_t flags, uint8_t lun, const T &commandBlock)
    : CommandBlockWrapper(tag, dataTransferLength, flags, lun, sizeof(T), (const void *) &commandBlock) {}
} __attribute__((__packed__));
static_assert(sizeof(CommandBlockWrapper) == 0x1F);

struct CommandStatusWrapper {
    uint32_t dCSWSignature;
    uint32_t dCSWTag;
    uint32_t dCSWDataResidue;
    uint8_t dCSWStatus;

    CommandStatusWrapper() : dCSWSignature(0), dCSWTag(0), dCSWDataResidue(0), dCSWStatus(0xFF) {}
    bool IsValid() const {
        return dCSWSignature == 0x53425355;
    }
    bool IsSuccessful() const {
        return dCSWStatus == 0;
    }
    bool IsCommandFailed() const {
        return dCSWStatus == 1;
    }
    bool IsPhaseError() const {
        return dCSWStatus == 2;
    }
} __attribute__((__packed__));
static_assert(sizeof(CommandStatusWrapper) == 13);

#define USBSTORAGE_FLAG_IN  0x80
#define USBSTORAGE_FLAG_OUT 0


class usbstorage_command_impl : public usbstorage_command {
private:
    usbstorage &device;
    void *buffer;
    CommandStatusWrapper status;
    uint8_t flags;
public:
    usbstorage_command_impl(usbstorage &device, uint32_t dataTransferLength, const scsivariabledata &varlength, uint8_t lun, const void *cmd,
                            uint8_t cmdLength, const std::function<void ()> &done)
    : usbstorage_command(dataTransferLength, varlength, lun, cmd, cmdLength, true, done), device(device), buffer(nullptr),
      flags(USBSTORAGE_FLAG_IN) {
        if (dataTransferLength != 0) {
            buffer = malloc(dataTransferLength);
        }
    }
    ~usbstorage_command_impl() {
        if (buffer != nullptr) {
            free(buffer);
            buffer = nullptr;
        }
    }
    void Transfer(const void *data, std::size_t size, const std::function<void ()> &done);
    void InTransfer(void *data, std::size_t size, const std::function<void (usb_transfer_status, std::size_t)> &done);
    void Start();
    void DataStage();
    void LookForStatusStage(std::size_t remaining);
    void ClearStallAndThenStatusStage();
    void StatusStage();

    const void *Buffer() const override;
    bool IsSuccessful() override;
};

void usbstorage_command_impl::Transfer(const void *data, std::size_t size, const std::function<void ()> &doneCallback) {
#ifdef USBSTORAGE_DEBUG_TRANSFERS
    {
        std::stringstream str{};
        str << "OUT " << size << "\n";
        get_klogger() << str.str().c_str();
    }
#endif
    std::size_t chunkSize{device.devInfo.port.TransferBufferSize()};
    chunkSize -= chunkSize % device.bulkOutDesc.wMaxPacketSize;
    const uint8_t *ptr = (const uint8_t *) data;
    while (size > 0) {
        std::size_t s = size;
        if (s > chunkSize) {
            s = chunkSize;
        }
        size -= s;
        std::shared_ptr<std::shared_ptr<usb_transfer>> transferIndirect =
                std::make_shared<std::shared_ptr<usb_transfer>>();
        std::shared_ptr<std::function<void ()>> done{new std::function<void ()>(doneCallback)};
        *transferIndirect = device.bulkOutEndpoint->CreateTransferWithLock(size == 0, (const void *) ptr, s, usb_transfer_direction::OUT, [size, done, transferIndirect] () {
            auto transfer = *transferIndirect;
            if (transfer->GetStatus() != usb_transfer_status::NO_ERROR) {
                std::stringstream str{};
                str << "Error: Usbstorage bulk OUT failed: " << transfer->GetStatusStr() << "\n";
                get_klogger() << str.str().c_str();
            }
            if (size == 0) {
                (*done)();
            }
        }, false, 0, device.bulkOutToggle);
        ptr += s;
        uint8_t packets = s / device.bulkOutDesc.wMaxPacketSize;
        if ((s % device.bulkOutDesc.wMaxPacketSize) != 0) {
            ++packets;
        }
        device.bulkOutToggle += packets;
    }
}

void usbstorage_command_impl::InTransfer(void *data, std::size_t size, const std::function<void (usb_transfer_status, std::size_t)> &doneCallback) {
#ifdef USBSTORAGE_DEBUG_TRANSFERS
    {
        std::stringstream str{};
        str << "IN " << size << " DT=" << ((device.bulkInToggle & 1) != 0 ? "1" : "0") << "\n";
        get_klogger() << str.str().c_str();
    }
#endif
    std::size_t chunkSize{device.devInfo.port.TransferBufferSize()};
    chunkSize -= chunkSize % device.bulkInDesc.wMaxPacketSize;
    uint8_t *ptr = (uint8_t *) data;
    auto totalSize = size;
    while (size > 0) {
        std::size_t s = size;
        if (s > chunkSize) {
            s = chunkSize;
        }
        std::size_t baseSize = (totalSize - size);
        size -= s;
        std::shared_ptr<std::shared_ptr<usb_transfer>> transferIndirect =
                std::make_shared<std::shared_ptr<usb_transfer>>();
        std::shared_ptr<std::function<void (usb_transfer_status, std::size_t)>> done{new std::function<void (usb_transfer_status, std::size_t)>(doneCallback)};
        uint8_t packets = s / device.bulkInDesc.wMaxPacketSize;
        if ((s % device.bulkInDesc.wMaxPacketSize) != 0) {
            ++packets;
        }
        *transferIndirect = device.bulkInEndpoint->CreateTransferWithLock(size == 0, s, usb_transfer_direction::IN, [this, ptr, s, size, transferIndirect, done, baseSize, packets] () {
            auto transfer = *transferIndirect;
            std::shared_ptr<usb_buffer> buffer{};
            if (transfer) {
                buffer = transfer->Buffer();
                auto actualTransferSize = transfer->Length();
                auto actualPackets = actualTransferSize / device.bulkInDesc.wMaxPacketSize;
                if ((actualTransferSize % device.bulkInDesc.wMaxPacketSize) != 0) {
                    actualPackets++;
                }
                auto diffPackets = packets - actualPackets;
                if (diffPackets != 0) {
                    device.bulkInToggle -= diffPackets;
                }
            }
            if (buffer && transfer->GetStatus() == usb_transfer_status::NO_ERROR) {
                memcpy((void *) ptr, buffer->Pointer(), s);
            } else {
                std::stringstream str{};
                str << "Error: Usbstorage bulk IN failed: " << transfer->GetStatusStr() << "\n";
                get_klogger() << str.str().c_str();
            }
            // TODO - error in part transfer should fail the whole, and probably reset dev
            if (size == 0) {
                (*done)(transfer->GetStatus(), baseSize + transfer->Length());
            }
        }, false, 0, device.bulkInToggle & 1);
        ptr += s;
        device.bulkInToggle += packets;
    }
}

void usbstorage_command_impl::Start() {
    CommandBlockWrapper cmd{device.commandTag++, dataTransferLength, flags, lun, cmdLength, (const void *) &(this->cmd[0])};
    Transfer((const void *) &cmd, sizeof(cmd), [] () {});
    DataStage();
}

void usbstorage_command_impl::DataStage() {
    if (dataTransferLength > 0 && buffer != nullptr) {
        if (inbound) {
            std::size_t initialTransfer = varlength->InitialRead(dataTransferLength);
            auto packetSize = device.bulkInDesc.wMaxPacketSize;
            if (initialTransfer != dataTransferLength && initialTransfer < packetSize) {
                initialTransfer = packetSize;
                if (initialTransfer > dataTransferLength) {
                    initialTransfer = dataTransferLength;
#ifdef USBSTORAGE_COMMAND_DEBUG
                    std::stringstream str{};
                    str << "Initial transfer of " << initialTransfer << "\n";
                    get_klogger() << str.str().c_str();
#endif
                }
            }
            if (initialTransfer == dataTransferLength) {
                InTransfer(buffer, dataTransferLength, [this] (usb_transfer_status transferStatus, std::size_t size) {
                    if (size < dataTransferLength) {
                        auto remaining = dataTransferLength - size;
#ifdef USBSTORAGE_DEBUG_SHORT_READ
                        {
                            std::stringstream str{};
                            str << "Short read " << size << "<" << dataTransferLength << ", will try remaining " << remaining << " while looking for status stage\n";
                            get_klogger() << str.str().c_str();
                        }
#endif
                        auto totalSize = varlength->TotalSize(buffer, size, dataTransferLength);
#ifdef USBSTORAGE_DEBUG_SHORT_READ
                        {
                            std::stringstream str{};
                            str << "Initial read of " << size << " and turns out to be of total size " << totalSize << "\n";
                            get_klogger() << str.str().c_str();
                        }
#endif
#ifdef USBSTORAGE_EXPECT_SHORT_READ
                        remaining = totalSize - size;
                        if (totalSize > size) {
#else
                        if (remaining > 0) {
#endif
                            LookForStatusStage(remaining);
                        } else {
                            StatusStage();
                        }
                    } else {
                        StatusStage();
                    }
                });
                return;
            } else {
                InTransfer(buffer, initialTransfer, [this, initialTransfer, packetSize] (usb_transfer_status transferStatus, std::size_t size) {
                    std::size_t remaining = varlength->Remaining(buffer, size, dataTransferLength);
                    std::size_t remainingExpected = dataTransferLength - initialTransfer;
                    if (remaining > 0) {
                        std::size_t total{initialTransfer + remaining};
#ifdef USBSTORAGE_COMMAND_DEBUG
                        {
                            std::stringstream str{};
                            str << "Remaining is " << remaining << " original was " << remainingExpected << "\n";
                            get_klogger() << str.str().c_str();
                        }
#endif
                        if (remaining < remainingExpected) {
                            auto original = remaining;
#ifdef USBSTORAGE_COMMAND_DEBUG
                            std::stringstream str{};
#endif
                            auto mod = (remaining % packetSize);
                            if (mod != 0) {
#ifdef USBSTORAGE_COMMAND_DEBUG
                                str << "Remaining of " << remaining << " rounded up to ";
#endif
                                remaining += packetSize - mod;
#ifdef USBSTORAGE_COMMAND_DEBUG
                                str << remaining;
#endif
                            }
                            if (remaining > remainingExpected) {
                                remaining = remainingExpected;
#ifdef USBSTORAGE_COMMAND_DEBUG
                                str << " and clipped to " << remaining;
#endif
                            }
#ifdef USBSTORAGE_COMMAND_DEBUG
                            if (original != remaining) {
                                str << "\n";
                                get_klogger() << str.str().c_str();
                            }
#endif
                        }
                        InTransfer(((uint8_t *) buffer) + initialTransfer, remaining, [](usb_transfer_status transferStatus, std::size_t size) {});
                    }
                    auto totalSize = varlength->TotalSize(buffer, size, dataTransferLength);
#ifdef USBSTORAGE_DEBUG_SHORT_READ
                    {
                        std::stringstream str{};
                        str << "Initial read of " << size << " and turns out to be of total size " << totalSize << "\n";
                        get_klogger() << str.str().c_str();
                    }
#endif
                    if (remaining < remainingExpected) {
                        auto fuzzyStage = remainingExpected - remaining;
#ifdef USBSTORAGE_COMMAND_DEBUG
                        std::stringstream str{};
                        str << "Location of status stage searching between 0-"<<fuzzyStage << "\n";
                        get_klogger() << str.str().c_str();
#endif
                        LookForStatusStage(fuzzyStage);
                        return;
                    }
                    StatusStage();
#ifdef USBSTORAGE_COMMAND_DEBUG
                    std::stringstream str{};
                    str << "Remaining transfer of " << remaining << " queued\n";
                    get_klogger() << str.str().c_str();
#endif
                });
#ifdef USBSTORAGE_COMMAND_DEBUG
                std::stringstream str{};
                str << "Initial transfer of " << initialTransfer << " queued\n";
                get_klogger() << str.str().c_str();
#endif
                return;
            }
        } else {
            Transfer(buffer, dataTransferLength, [] () {});
        }
    }
    StatusStage();
}

struct TmpBuffer {
    void *ptr;

    TmpBuffer(std::size_t size) : ptr{malloc(size)} {
    }
    ~TmpBuffer() {
        free(ptr);
    }
    TmpBuffer(const TmpBuffer &) = delete;
    TmpBuffer(TmpBuffer &&) = delete;
    TmpBuffer &operator = (const TmpBuffer &) = delete;
    TmpBuffer &operator = (TmpBuffer &&) = delete;
};

void usbstorage_command_impl::LookForStatusStage(std::size_t remaining) {
    auto packetSize = device.bulkInDesc.wMaxPacketSize;
    std::shared_ptr<TmpBuffer> buffer{new TmpBuffer(packetSize > sizeof(status) ? packetSize : sizeof(status))};
    if (remaining > 0) {
        if (sizeof(status) > packetSize) {
            if (remaining < packetSize) {
                remaining = packetSize;
            }
        } else if (remaining < sizeof(status)) {
            remaining = sizeof(status);
        }
        auto transfer = remaining;
        if (transfer > packetSize) {
            transfer = packetSize;
        }
        InTransfer(buffer->ptr, transfer, [this, packetSize, transfer, buffer, remaining] (usb_transfer_status transferStatus, std::size_t sizeRead) {
            if (transferStatus == usb_transfer_status::STALL) {
                ClearStallAndThenStatusStage();
                return;
            }
            auto size = transfer;
            if (size > sizeof(status)) {
                size = sizeof(status);
                if (size > packetSize) {
                    size = packetSize;
                }
            }
            memcpy(&status, buffer->ptr, size);
            if (status.IsValid()) {
#ifdef USBSTORAGE_COMMAND_DEBUG
                std::stringstream str{};
                str << "Found status\n";
                get_klogger() << str.str().c_str();
#endif

                if (sizeof(status) < packetSize) {
                    InTransfer(((uint8_t *) (buffer->ptr)) + packetSize, sizeof(status) - packetSize, [this, buffer] (usb_transfer_status transferStatus, std::size_t sizeRead) {
                        memcpy(&status, buffer->ptr, sizeof(status));

                        std::function<void ()> done = this->done;
                        device.ExecuteQueueItem();
                        done();
                    });
                    return;
                }

                std::function<void ()> done = this->done;
                device.ExecuteQueueItem();
                done();
            } else {
#ifdef USBSTORAGE_COMMAND_DEBUG
                std::stringstream str{};
#endif
                if (transfer < remaining) {
                    auto fuzzyStage = remaining - transfer;
#ifdef USBSTORAGE_COMMAND_DEBUG
                    str << "Continue searching between 0-" << fuzzyStage << "\n";
                    get_klogger() << str.str().c_str();
#endif
                    LookForStatusStage(fuzzyStage);
                } else {
#ifdef USBSTORAGE_COMMAND_DEBUG
                    str << "Full length search\n";
                    get_klogger() << str.str().c_str();
#endif
                    StatusStage();
                }
            }
        });
    }
}

void usbstorage_command_impl::ClearStallAndThenStatusStage() {
    device.QueueTask([this] () {
        {
            std::stringstream str{};
            str << device.DeviceType() << device.DeviceId() << ": Clearing IN endpoint stall\n";
            get_klogger() << str.str().c_str();
        }
        if (!device.bulkInEndpoint->ClearStall()) {
            std::stringstream str{};
            str << device.DeviceType() << device.DeviceId() << ": Clear stall failed\n";
            get_klogger() << str.str().c_str();
        }
        if (device.devInfo.port.ControlRequest(*(device.devInfo.port.Endpoint0()), usb_clear_feature(usb_feature::ENDPOINT_HALT, usb_control_endpoint_direction::IN, device.bulkInDesc.Address()))) {
            std::stringstream str{};
            str << device.DeviceType() << device.DeviceId() << ": Cleared device stall condition\n";
            get_klogger() << str.str().c_str();
            device.bulkInToggle = 0;
        } else {
            std::stringstream str{};
            str << device.DeviceType() << device.DeviceId() << ": Clear device stall failed\n";
            get_klogger() << str.str().c_str();
        }
        {
            std::lock_guard lock{device.devInfo.port.Hub().HcdSpinlock()};
            StatusStage();
        }
    });
}

void usbstorage_command_impl::StatusStage() {
    InTransfer(&status, sizeof(status), [this] (usb_transfer_status transferStatus, std::size_t sizeRead) {
        if (transferStatus == usb_transfer_status::STALL) {
            device.QueueTask([this] () {
                {
                    std::stringstream str{};
                    str << device.DeviceType() << device.DeviceId() << ": Clearing IN endpoint status-stall\n";
                    get_klogger() << str.str().c_str();
                }
                if (!device.bulkInEndpoint->ClearStall()) {
                    std::stringstream str{};
                    str << device.DeviceType() << device.DeviceId() << ": Clear status-stall failed\n";
                    get_klogger() << str.str().c_str();
                }
                if (device.devInfo.port.ControlRequest(*(device.devInfo.port.Endpoint0()), usb_clear_feature(usb_feature::ENDPOINT_HALT, usb_control_endpoint_direction::IN, device.bulkInDesc.Address()))) {
                    std::stringstream str{};
                    str << device.DeviceType() << device.DeviceId() << ": Cleared device stall condition\n";
                    get_klogger() << str.str().c_str();
                    device.bulkInToggle = 0;
                } else {
                    std::stringstream str{};
                    str << device.DeviceType() << device.DeviceId() << ": Clear device stall failed\n";
                    get_klogger() << str.str().c_str();
                }
                {
                    std::lock_guard lock{device.devInfo.port.Hub().HcdSpinlock()};
                    InTransfer(&status, sizeof(status), [this] (usb_transfer_status transferStatus, std::size_t sizeRead) {
                        std::function<void()> done = this->done;
                        device.ExecuteQueueItem();
                        done();
                    });
                }
            });
        } else {
            std::function<void()> done = this->done;
            device.ExecuteQueueItem();
            done();
        }
    });
}

const void *usbstorage_command_impl::Buffer() const {
    return buffer;
}

bool usbstorage_command_impl::IsSuccessful() {
    if (!status.IsValid()) {
        nonSuccessfulStatus = ScsiCmdNonSuccessfulStatus::SIGNATURE_CHECK;
        nonSuccessfulStatusString = "SIGNATURE_CHECK";
        return false;
    }
    if (!status.IsSuccessful()) {
        if (status.IsCommandFailed()) {
            nonSuccessfulStatus = ScsiCmdNonSuccessfulStatus::COMMAND_FAILED;
            nonSuccessfulStatusString = "COMMMAND_FAILED";
            return false;
        }
        if (status.IsPhaseError()) {
            nonSuccessfulStatus = ScsiCmdNonSuccessfulStatus::PHASE_ERROR;
            nonSuccessfulStatusString = "PHASE_ERROR";
            return false;
        }
        nonSuccessfulStatus = ScsiCmdNonSuccessfulStatus::OTHER_ERROR;
        nonSuccessfulStatusString = "OTHER_ERROR";
        return false;
    }
    return true;
}

class usbstorage_scsi_dev : public ScsiDevDeviceInformation {
private:
    usbstorage &device;
    uint8_t lun;
public:
    usbstorage_scsi_dev(usbstorage &device, uint8_t lun) : ScsiDevDeviceInformation(), device(device), lun(lun) {
    }
    std::shared_ptr<ScsiDevDeviceInformation> Clone() override;
    ScsiDevDeviceInformation *GetScsiDevDeviceInformation() override;
    uint8_t GetLun() const override;
    std::shared_ptr<InquiryResult> GetInquiryResult() override;
    void SetDevice(Device *device) override;
    std::shared_ptr<ScsiDevCommand> ExecuteCommand(const void *cmd, std::size_t cmdLength, std::size_t dataTransferLength, const scsivariabledata &varlength, const std::function<void ()> &done) override;
    bool ResetDevice() override;
};

std::shared_ptr<ScsiDevDeviceInformation> usbstorage_scsi_dev::Clone() {
    std::shared_ptr<ScsiDevDeviceInformation> clone{new usbstorage_scsi_dev(device, lun)};
    return clone;
}

ScsiDevDeviceInformation *usbstorage_scsi_dev::GetScsiDevDeviceInformation() {
    return this;
}

uint8_t usbstorage_scsi_dev::GetLun() const {
    return lun;
}

std::shared_ptr<InquiryResult> usbstorage_scsi_dev::GetInquiryResult() {
    return {};
}

void usbstorage_scsi_dev::SetDevice(Device *device) {
    this->device.SetDevice(device);
}

class usbstorage_scsi_devcmd : public ScsiDevCommand {
private:
    std::shared_ptr<usbstorage_command> usbstoragecmd;
public:
    usbstorage_scsi_devcmd(std::shared_ptr<usbstorage_command> usbstoragecmd) : usbstoragecmd(usbstoragecmd) {}
    const void *Buffer() const override;
    std::size_t Size() const override;
    bool IsSuccessful() override;
    ScsiCmdNonSuccessfulStatus NonSuccessfulStatus() const override;
    const char *NonSuccessfulStatusString() const override;
};

const void *usbstorage_scsi_devcmd::Buffer() const {
    return usbstoragecmd->Buffer();
}

std::size_t usbstorage_scsi_devcmd::Size() const {
    return usbstoragecmd->DataTransferLength();
}

bool usbstorage_scsi_devcmd::IsSuccessful() {
    return usbstoragecmd->IsSuccessful();
}

ScsiCmdNonSuccessfulStatus usbstorage_scsi_devcmd::NonSuccessfulStatus() const {
    return usbstoragecmd->NonSuccessfulStatus();
}

const char *usbstorage_scsi_devcmd::NonSuccessfulStatusString() const {
    return usbstoragecmd->NonSucessfulStatusString();
}

std::shared_ptr<ScsiDevCommand> usbstorage_scsi_dev::ExecuteCommand(const void *cmd, std::size_t cmdLength, std::size_t dataTransferLength, const scsivariabledata &varlength, const std::function<void ()> &done) {
    std::shared_ptr<usbstorage_scsi_devcmd> subcmd =
            std::make_shared<usbstorage_scsi_devcmd>(device.QueueCommand(dataTransferLength, varlength, lun, cmd, cmdLength, done));
    std::shared_ptr<ScsiDevCommand> scsiDevCommand{subcmd};
    return scsiDevCommand;
}

bool usbstorage_scsi_dev::ResetDevice() {
    return device.ResetDevice();
}

Device *usbstorage_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (deviceInformation.device_class == 0 && deviceInformation.device_subclass == 0 && deviceInformation.prog_if == 0) {
        auto *usbInfo = deviceInformation.GetUsbInformation();
        if (usbInfo != nullptr) {
            UsbIfacedevInformation *ifaceDev{nullptr};
            {
                UsbInterfaceInformation *ifaceInfo = usbInfo->GetUsbInterfaceInformation();
                if (ifaceInfo != nullptr) {
                    ifaceDev = ifaceInfo->GetIfaceDev();
                }
            }
            if (ifaceDev != nullptr && ifaceDev->iface.bInterfaceClass == 8 && ifaceDev->iface.bInterfaceProtocol == IFACE_PROTO_BULK_ONLY) {
                auto *dev = new usbstorage(&bus, *ifaceDev, ifaceDev->iface.bInterfaceSubClass);
                return dev;
            }
        }
    }
    return nullptr;
}

void usbstorage::init() {
    std::shared_ptr<usb_endpoint> endpoint0 = devInfo.port.Endpoint0();
    if (!devInfo.port.SetConfigurationValue(devInfo.descr.bConfigurationValue, 0, 0)) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Error: USB storage set configuration failed\n";
        get_klogger() << str.str().c_str();
        return;
    }
    {
        int maxLun = GetMaxLun();
        if (maxLun < 0) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Error: USB storage get max lun failed\n";
            get_klogger() << str.str().c_str();
            return;
        }
        this->maxLun = maxLun;
    }

    {
        bool bulkInFound{false};
        bool bulkOutFound{false};
        for (const auto &endpoint: devInfo.endpoints) {
            if (endpoint.IsBulk()) {
                if (endpoint.IsDirectionIn()) {
                    bulkInDesc = endpoint;
                    bulkInFound = true;
                } else {
                    bulkOutDesc = endpoint;
                    bulkOutFound = true;
                }
            }
        }
        if (!bulkInFound || !bulkOutFound) {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": Bulk endpoint, ";
            if (bulkInFound) {
                str << "out";
            } else if (bulkOutFound) {
                str << "in";
            } else {
                str << "both";
            }
            str << ", not found\n";
            get_klogger() << str.str().c_str();
            return;
        }
    }
    bulkInEndpoint = devInfo.port.BulkEndpoint(bulkInDesc.wMaxPacketSize, bulkInDesc.bEndpointAddress, usb_endpoint_direction::IN);
    bulkOutEndpoint = devInfo.port.BulkEndpoint(bulkOutDesc.wMaxPacketSize, bulkOutDesc.bEndpointAddress, usb_endpoint_direction::OUT);
    if (!bulkInEndpoint || !bulkOutEndpoint) {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": Failed to create bulk endpoint, ";
        if (bulkInEndpoint) {
            str << "out";
        } else if (bulkOutEndpoint) {
            str << "in";
        } else {
            str << "both";
        }
        str << "\n";
        get_klogger() << str.str().c_str();
        return;
    }

    if (taskRunner == nullptr) {
        taskRunner = new std::thread([this] () {
            {
                std::stringstream str{};
                str << "[" << DeviceType() << DeviceId() << "]";
                std::this_thread::set_name(str.str());
            }
            RunTasks();
        });
    }

    std::stringstream str{};
    str << DeviceType() << DeviceId() << ": subclass=" << subclass << " luns=" << (((int) maxLun) + 1) << "\n";
    get_klogger() << str.str().c_str();

    for (int lun = 0; lun <= maxLun; lun++) {
        if (subclass == USBSTORAGE_SUBCLASS_SCSI) {
            usbstorage_scsi_dev scsiDev{*this, (uint8_t) lun};
            get_drivers().probe(*this, scsiDev);
        }
    }
}

void usbstorage::stop() {
    if (taskRunner != nullptr) {
        {
            std::lock_guard lock{taskQueueLock};
            stopThreads = true;
            taskAvailable.release();
        }
        taskRunner->join();
        delete taskRunner;
        taskRunner = nullptr;
    }
    if (device != nullptr) {
        device->stop();
        delete device;
        device = nullptr;
    }
}

void usbstorage::SetDevice(Device *device) {
    this->device = device;
}

usbstorage::~usbstorage() {
    if (taskRunner != nullptr) {
        {
            std::lock_guard lock{taskQueueLock};
            stopThreads = true;
            taskAvailable.release();
        }
        taskRunner->join();
        delete taskRunner;
        taskRunner = nullptr;
    }
    if (device != nullptr) {
        device->stop();
        delete device;
        device = nullptr;
    }
}

bool usbstorage::MassStorageReset() {
    std::shared_ptr<usb_endpoint> endpoint0 = devInfo.port.Endpoint0();
    if (!devInfo.port.ControlRequest(*endpoint0, ustorage_mass_reset(devInfo.iface.bInterfaceNumber))) {
        return false;
    }
    return true;
}

int usbstorage::GetMaxLun() {
    std::shared_ptr<usb_endpoint> endpoint0 = devInfo.port.Endpoint0();
    auto buffer = devInfo.port.ControlRequest(*endpoint0, ustorage_get_max_lun(devInfo.iface.bInterfaceNumber));
    if (!buffer) {
        return -1;
    }
    uint8_t maxLun;
    buffer->CopyTo(maxLun);
    return maxLun;
}

std::shared_ptr<usbstorage_command>
usbstorage::QueueCommand(uint32_t dataTransferLength, const scsivariabledata &varlength, uint8_t lun, const void *cmd, uint8_t cmdLength, const std::function<void ()> &done) {
    std::shared_ptr<usbstorage_command_impl> cmd_pt{new usbstorage_command_impl(*this, dataTransferLength, varlength, lun, cmd, cmdLength, done)};

    {
        std::lock_guard lock{devInfo.port.Hub().HcdSpinlock()};
        commandQueue.push_back(cmd_pt);
        if (!currentCommand) {
            ExecuteQueueItem();
        }
    }
    std::shared_ptr<usbstorage_command> cmdsh{cmd_pt};
    return cmdsh;
}

bool usbstorage::ResetDevice() {
    {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": resetting\n";
        get_klogger() << str.str().c_str();
    }
    if (MassStorageReset()) {
        {
            std::stringstream str{};
            str << DeviceType() << DeviceId() << ": mass storage reset 1 successful\n";
            get_klogger() << str.str().c_str();
        }
        {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(1s);
        }
        if (bulkOutEndpoint->CancelAllTransfers() && bulkInEndpoint->CancelAllTransfers()) {
            {
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": cancelled current transfers\n";
                get_klogger() << str.str().c_str();
            }
            auto massReset = MassStorageReset();
            {
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": mass storage reset "
                    << (massReset ? "successful\n" : "failed\n");
                get_klogger() << str.str().c_str();
            }
            return massReset;
        }
    } else {
        std::stringstream str{};
        str << DeviceType() << DeviceId() << ": mass storage reset failed\n";
        get_klogger() << str.str().c_str();
    }
    return false;
}

void usbstorage::QueueTask(const std::function<void()> &task) {
    std::lock_guard lock{taskQueueLock};
    taskQueue.push_back(task);
    taskAvailable.release();
}

void usbstorage::ExecuteQueueItem() {
    {
        auto iterator = commandQueue.begin();
        if (iterator == commandQueue.end()) {
            currentCommand = {};
            return;
        }
        currentCommand = *iterator;
        commandQueue.erase(iterator);
    }
    currentCommand->Start();
}

void usbstorage::RunTasks() {
    while (true) {
        taskAvailable.acquire();
        std::vector<std::function<void ()>> run{};
        {
            std::lock_guard lock{taskQueueLock};
            if (stopThreads) {
                return;
            }
            for (const auto &task: taskQueue) {
                run.push_back(task);
            }
            taskQueue.clear();
        }
        for (auto &task : run) {
            task();
        }
    }
}
