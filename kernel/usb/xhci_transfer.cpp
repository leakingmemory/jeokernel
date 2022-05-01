//
// Created by sigsegv on 12/11/21.
//

#include <sstream>
#include "xhci_transfer.h"

std::shared_ptr<usb_buffer> xhci_transfer::Buffer() {
    return buffer;
}

usb_transfer_status xhci_transfer::GetStatus() {
    return status;
}

#define XHCI_ERROR_INVALID                      0
#define XHCI_ERROR_SUCCESS                      1
#define XHCI_ERROR_DATA_BUFFER_ERROR            2
#define XHCI_ERROR_BABBLE_DETECTED              3
#define XHCI_ERROR_TRANSACTION_ERROR            4
#define XHCI_ERROR_TRB_ERROR                    5
#define XHCI_ERROR_STALL_ERROR                  6
#define XHCI_ERROR_RESOURCE_ERROR               7
#define XHCI_ERROR_BANDWIDTH_ERROR              8
#define XHCI_ERROR_NO_SLOTS_AVAIL               9
#define XHCI_ERROR_INVALID_STREAM_TYPE          10
#define XHCI_ERROR_SLOT_NOT_ENABLED             11
#define XHCI_ERROR_ENDPOINT_NOT_ENABLED         12
#define XHCI_ERROR_SHORT_PACKET                 13
#define XHCI_ERROR_RING_UNDERRUN                14
#define XHCI_ERROR_RING_OVERRUN                 15
#define XHCI_ERROR_VF_EVENT_RING_FULL           16
#define XHCI_ERROR_PARAMETER_ERROR              17
#define XHCI_ERROR_BANDWIDTH_OVERRUN            18
#define XHCI_ERROR_CONTEXT_STATE                19
#define XHCI_ERROR_NO_PING_RESPONSE             20
#define XHCI_ERROR_EVENT_RING_FULL              21
#define XHCI_ERROR_INCOMPATIBLE_DEVICE          22
#define XHCI_ERROR_MISSED_SERVICE               23
#define XHCI_ERROR_COMMAND_RING_STOPPED         24
#define XHCI_ERROR_COMMAND_ABORTED              25
#define XHCI_ERROR_STOPPED                      26
#define XHCI_ERROR_STOPPED_LENGTH_INVALID       27
#define XHCI_ERROR_STOPPED_SHORT_PACKET         28
#define XHCI_ERROR_MAX_EXIT_LATENCY_TOO_LARGE   29
#define XHCI_ERROR_ISOCH_BUFFER_OVERRUN         31
#define XHCI_ERROR_EVENT_LOST                   32
#define XHCI_ERROR_UNDEFINED_ERROR              33
#define XHCI_ERROR_INVALID_STREAM_ID            34
#define XHCI_ERROR_SECONDARY_BANDWIDTH          35
#define XHCI_ERROR_SPLIT_TRANSACTION            36
void xhci_transfer::SetStatus(uint8_t status, std::size_t length) {
    if (status != XHCI_ERROR_SUCCESS) {
        std::stringstream str{};
        str << "XHCI transfer "<<std::hex<< this->physAddr << "/" << ((uint64_t) this)<<std::dec<< " error code " << status << "\n";
        get_klogger() << str.str().c_str();
    }
    switch (status) {
        case XHCI_ERROR_SUCCESS:
            this->status = usb_transfer_status::NO_ERROR;
            break;
        case XHCI_ERROR_DATA_BUFFER_ERROR:
            this->status = usb_transfer_status::CRC;
            break;
        case XHCI_ERROR_BABBLE_DETECTED:
        case XHCI_ERROR_TRANSACTION_ERROR:
        case XHCI_ERROR_RESOURCE_ERROR:
        case XHCI_ERROR_BANDWIDTH_ERROR:
        case XHCI_ERROR_CONTEXT_STATE:
        case XHCI_ERROR_NO_PING_RESPONSE:
        case XHCI_ERROR_INCOMPATIBLE_DEVICE:
        case XHCI_ERROR_MISSED_SERVICE:
        case XHCI_ERROR_COMMAND_RING_STOPPED:
        case XHCI_ERROR_COMMAND_ABORTED:
        case XHCI_ERROR_STOPPED:
        case XHCI_ERROR_STOPPED_LENGTH_INVALID:
        case XHCI_ERROR_STOPPED_SHORT_PACKET:
            this->status = usb_transfer_status::DEVICE_NOT_RESPONDING;
            break;
        case XHCI_ERROR_TRB_ERROR:
        case XHCI_ERROR_NO_SLOTS_AVAIL:
        case XHCI_ERROR_INVALID_STREAM_TYPE:
        case XHCI_ERROR_SLOT_NOT_ENABLED:
        case XHCI_ERROR_ENDPOINT_NOT_ENABLED:
        case XHCI_ERROR_PARAMETER_ERROR:
        case XHCI_ERROR_MAX_EXIT_LATENCY_TOO_LARGE:
            this->status = usb_transfer_status::DATA_TOGGLE_MISMATCH;
            break;
        case XHCI_ERROR_STALL_ERROR:
            this->status = usb_transfer_status::STALL;
            break;
        case XHCI_ERROR_SHORT_PACKET:
            {
                auto expectedLength = Length();
                if (length > 0 && length <= expectedLength) {
                    std::stringstream str{};
                    str << "Short packet, expected " << expectedLength << ", remaining " << length << ", actually ";
                    expectedLength -= length;
                    str << expectedLength << "\n";
                    get_klogger() << str.str().c_str();
                    Length(expectedLength);
                    this->status = usb_transfer_status::NO_ERROR;
                    break;
                }
            }
        case XHCI_ERROR_RING_UNDERRUN:
            this->status = usb_transfer_status::DATA_UNDERRUN;
            break;
        case XHCI_ERROR_RING_OVERRUN:
        case XHCI_ERROR_VF_EVENT_RING_FULL:
        case XHCI_ERROR_BANDWIDTH_OVERRUN:
        case XHCI_ERROR_EVENT_RING_FULL:
        case XHCI_ERROR_ISOCH_BUFFER_OVERRUN:
        case XHCI_ERROR_EVENT_LOST:
            this->status = usb_transfer_status::DATA_OVERRUN;
            break;
        case XHCI_ERROR_INVALID:
        case XHCI_ERROR_UNDEFINED_ERROR:
        case XHCI_ERROR_INVALID_STREAM_ID:
        case XHCI_ERROR_SECONDARY_BANDWIDTH:
        case XHCI_ERROR_SPLIT_TRANSACTION:
        default:
            this->status = usb_transfer_status::UNKNOWN_ERROR;
            break;
    }
    SetDone();
}
