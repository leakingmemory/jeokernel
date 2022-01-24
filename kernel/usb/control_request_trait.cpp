//
// Created by sigsegv on 12/11/21.
//

#include <chrono>
#include <thread>
#include <sstream>
#include "control_request_trait.h"
#include "usb_transfer.h"
#include "usb_port_connection.h"

static void timeout_wait(const usb_transfer &transfer) {
    int timeout = 25;
    while (!transfer.IsDone()) {
        if (--timeout == 0)
            break;
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(40ms);
    }
}

class long_usb_buffer : public usb_buffer {
private:
    void *buffer;
    size_t size;
public:
    long_usb_buffer(const std::vector<std::shared_ptr<usb_buffer>> &buffers) : buffer(nullptr), size{0} {
        for (const std::shared_ptr<usb_buffer> &buf : buffers) {
            size += buf->Size();
        }
        buffer = malloc(size);
        size_t offset{0};
        for (const std::shared_ptr<usb_buffer> &buf : buffers) {
            memcpy(((uint8_t *) buffer) + offset, buf->Pointer(), buf->Size());
            offset += buf->Size();
        }
    }
    ~long_usb_buffer() {
        if (buffer != nullptr) {
            free(buffer);
            buffer = nullptr;
        }
    }

    void *Pointer() override {
        return buffer;
    }
    uint64_t Addr() override {
        return 0;
    }
    size_t Size() override {
        return size;
    }
};

std::shared_ptr<usb_buffer>
control_request_trait::ControlRequest(usb_endpoint &endpoint, const usb_control_request &request) {
    std::shared_ptr<usb_transfer> t_req = endpoint.CreateTransfer(
            false, (void *) &request, sizeof(request),
            usb_transfer_direction::SETUP, true,
            1, 0
    );
    std::vector<std::shared_ptr<usb_transfer>> data_stage{};
    std::shared_ptr<usb_transfer> status_stage{};
    if (request.wLength > 0) {
        auto remaining = request.wLength;
        while (remaining > 0) {
            auto data_stage_tr = endpoint.CreateTransfer(
                    false, request.wLength,
                    usb_transfer_direction::IN, true,
                    1, 1);
            data_stage.push_back(data_stage_tr);
            auto size = data_stage_tr->Buffer()->Size();
            if (size < remaining) {
                remaining -= size;
            } else {
                remaining = 0;
            }
        }
        status_stage = endpoint.CreateTransfer(
                true, 0,
                usb_transfer_direction::OUT, true,
                1, 1
        );
    } else {
        status_stage = endpoint.CreateTransfer(
                true, 0,
                usb_transfer_direction::IN, true,
                1, 1
        );
    }
    timeout_wait(*t_req);
    if (!t_req->IsSuccessful()) {
        std::stringstream str{};
        str << "Usb control transfer failed: " << t_req->GetStatusStr() << "\n";
        get_klogger() << str.str().c_str();
        return {};
    }
    if (!data_stage.empty()) {
        for (const auto &stage : data_stage) {
            timeout_wait(*stage);
            if (!stage->IsSuccessful()) {
                std::stringstream str{};
                str << "Usb control transfer failed (data): " << stage->GetStatusStr() << "\n";
                get_klogger() << str.str().c_str();
                return {};
            }
        }
    }
    timeout_wait(*status_stage);
    if (!status_stage->IsSuccessful()) {
        std::stringstream str{};
        str << "Usb control transfer failed (status): " << status_stage->GetStatusStr() << "\n";
        get_klogger() << str.str().c_str();
        return {};
    }
    if (!data_stage.empty()) {
        std::vector<std::shared_ptr<usb_buffer>> buffers{};
        for (const auto &stage : data_stage) {
            buffers.push_back(stage->Buffer());
        }
        return std::shared_ptr(new long_usb_buffer(buffers));
    } else {
        return t_req->Buffer();
    }
}

bool control_request_trait::ControlRequest(usb_endpoint &endpoint, const usb_control_request &request, void *data) {
    std::shared_ptr<usb_transfer> t_req = endpoint.CreateTransfer(
            false, (void *) &request, sizeof(request),
            usb_transfer_direction::SETUP, true,
            1, 0
    );
    if (!t_req) {
        std::stringstream str{};
        str << "Usb control transfer failed (setup): Failed to set up transfer\n";
        get_klogger() << str.str().c_str();
        return false;
    }
    std::shared_ptr<usb_transfer> data_stage_tr = endpoint.CreateTransfer(
            false, data, request.wLength,
            usb_transfer_direction::OUT, true,
            1, 1);
    if (!data_stage_tr) {
        std::stringstream str{};
        str << "Usb control transfer failed (data): Failed to set up transfer\n";
        get_klogger() << str.str().c_str();
        return false;
    }
    std::shared_ptr<usb_transfer> status_stage = endpoint.CreateTransfer(
            true, 0,
            usb_transfer_direction::IN, true,
            1, 1
    );
    if (!status_stage) {
        std::stringstream str{};
        str << "Usb control transfer failed (status): Failed to set up transfer\n";
        get_klogger() << str.str().c_str();
        return false;
    }
    timeout_wait(*t_req);
    if (!t_req->IsSuccessful()) {
        std::stringstream str{};
        str << "Usb control transfer failed: " << t_req->GetStatusStr() << "\n";
        get_klogger() << str.str().c_str();
        return false;
    }
    timeout_wait(*data_stage_tr);
    if (!data_stage_tr->IsSuccessful()) {
        std::stringstream str{};
        str << "Usb control transfer failed (data): " << data_stage_tr->GetStatusStr() << "\n";
        get_klogger() << str.str().c_str();
        return false;
    }
    timeout_wait(*status_stage);
    if (!status_stage->IsSuccessful()) {
        std::stringstream str{};
        str << "Usb control transfer failed (status): " << status_stage->GetStatusStr() << "\n";
        get_klogger() << str.str().c_str();
        return false;
    }
    return true;
}
