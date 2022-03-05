//
// Created by sigsegv on 2/26/22.
//

#include "usb_port_connection.h"
#include "usbstorage.h"
#include "usb_transfer.h"
#include <sstream>
#include "ustorage_structs.h"
#include "../scsi/scsidev.h"

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
    usbstorage_command_impl(usbstorage &device, uint32_t dataTransferLength, uint8_t lun, const void *cmd,
                            uint8_t cmdLength, const std::function<void ()> &done)
    : usbstorage_command(dataTransferLength, lun, cmd, cmdLength, true, done), device(device), buffer(nullptr),
      flags(USBSTORAGE_FLAG_IN) {
        buffer = malloc(dataTransferLength);
    }
    ~usbstorage_command_impl() {
        if (buffer != nullptr) {
            free(buffer);
            buffer = nullptr;
        }
    }
    void Transfer(const void *data, std::size_t size, const std::function<void ()> &done);
    void InTransfer(void *data, std::size_t size, const std::function<void ()> &done);
    void Start();
    void DataStage();
    void StatusStage();

    const void *Buffer() const override;
    bool IsSuccessful() const override;
};

void usbstorage_command_impl::Transfer(const void *data, std::size_t size, const std::function<void ()> &doneCallback) {
    std::size_t chunkSize{device.devInfo.port.TransferBufferSize()};
    chunkSize -= chunkSize % device.bulkOutDesc.wMaxPacketSize;
    const uint8_t *ptr = (const uint8_t *) data;
    while (size > 0) {
        std::size_t s = size;
        if (s > chunkSize) {
            s = chunkSize;
        }
        size -= s;
        std::shared_ptr<std::function<void ()>> done{new std::function<void ()>(doneCallback)};
        device.bulkOutEndpoint->CreateTransferWithLock(size == 0, (const void *) ptr, s, usb_transfer_direction::OUT, [size, done] () {
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

void usbstorage_command_impl::InTransfer(void *data, std::size_t size, const std::function<void ()> &doneCallback) {
    std::size_t chunkSize{device.devInfo.port.TransferBufferSize()};
    chunkSize -= chunkSize % device.bulkInDesc.wMaxPacketSize;
    uint8_t *ptr = (uint8_t *) data;
    while (size > 0) {
        std::size_t s = size;
        if (s > chunkSize) {
            s = chunkSize;
        }
        size -= s;
        std::shared_ptr<std::shared_ptr<usb_transfer>> transferIndirect =
                std::make_shared<std::shared_ptr<usb_transfer>>();
        std::shared_ptr<std::function<void ()>> done{new std::function<void ()>(doneCallback)};
        *transferIndirect = device.bulkInEndpoint->CreateTransferWithLock(size == 0, s, usb_transfer_direction::IN, [ptr, s, size, transferIndirect, done] () {
            auto transfer = *transferIndirect;
            std::shared_ptr<usb_buffer> buffer{};
            if (transfer) {
                buffer = transfer->Buffer();
            }
            if (buffer) {
                memcpy((void *) ptr, buffer->Pointer(), s);
            }
            // TODO - error in part transfer should fail the whole, and probably reset dev
            if (size == 0) {
                (*done)();
            }
        }, false, 0, device.bulkInToggle);
        ptr += s;
        uint8_t packets = s / device.bulkInDesc.wMaxPacketSize;
        if ((s % device.bulkInDesc.wMaxPacketSize) != 0) {
            ++packets;
        }
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
            InTransfer(buffer, dataTransferLength, [] () {});
        } else {
            Transfer(buffer, dataTransferLength, [] () {});
        }
    }
    StatusStage();
}

void usbstorage_command_impl::StatusStage() {
    InTransfer(&status, sizeof(status), [this] () {
        done();
    });
}

const void *usbstorage_command_impl::Buffer() const {
    return buffer;
}

bool usbstorage_command_impl::IsSuccessful() const {
    return status.IsValid() && status.IsSuccessful();
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
    std::shared_ptr<ScsiDevCommand> ExecuteCommand(const void *cmd, std::size_t cmdLength, std::size_t dataTransferLength, const std::function<void ()> &done) override;
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
    bool IsSuccessful() const override;
};

const void *usbstorage_scsi_devcmd::Buffer() const {
    return usbstoragecmd->Buffer();
}

std::size_t usbstorage_scsi_devcmd::Size() const {
    return usbstoragecmd->DataTransferLength();
}

bool usbstorage_scsi_devcmd::IsSuccessful() const {
    return usbstoragecmd->IsSuccessful();
}

std::shared_ptr<ScsiDevCommand> usbstorage_scsi_dev::ExecuteCommand(const void *cmd, std::size_t cmdLength, std::size_t dataTransferLength, const std::function<void ()> &done) {
    std::shared_ptr<usbstorage_scsi_devcmd> subcmd =
            std::make_shared<usbstorage_scsi_devcmd>(device.QueueCommand(dataTransferLength, lun, cmd, cmdLength, done));
    std::shared_ptr<ScsiDevCommand> scsiDevCommand{subcmd};
    return scsiDevCommand;
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
    if (device != nullptr) {
        device->stop();
        delete device;
        device = nullptr;
    }
}

void usbstorage::SetDevice(Device *device) {
    this->device = device;
}

usbstorage::~usbstorage() noexcept {
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
usbstorage::QueueCommand(uint32_t dataTransferLength, uint8_t lun, const void *cmd, uint8_t cmdLength, const std::function<void ()> &done) {
    std::shared_ptr<usbstorage_command_impl> cmd_pt{new usbstorage_command_impl(*this, dataTransferLength, lun, cmd, cmdLength, done)};

    {
        critical_section cli{};
        std::lock_guard lock{devInfo.port.Hub().HcdSpinlock()};
        commandQueue.push_back(cmd_pt);
        if (!currentCommand) {
            ExecuteQueueItem();
        }
    }
    std::shared_ptr<usbstorage_command> cmdsh{cmd_pt};
    return cmdsh;
}

void usbstorage::ExecuteQueueItem() {
    {
        auto iterator = commandQueue.begin();
        if (iterator == commandQueue.end()) {
            return;
        }
        currentCommand = *iterator;
        commandQueue.erase(iterator);
    }
    currentCommand->Start();
}
