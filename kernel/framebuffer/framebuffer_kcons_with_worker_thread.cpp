//
// Created by sigsegv on 12/26/21.
//

#include <string>
#include "framebuffer_kcons_with_worker_thread.h"
#include "framebuffer_kconsole.h"

class framebuffer_kcons_cmd {
public:
    virtual ~framebuffer_kcons_cmd() { }
    virtual int GetEstimatedLinefeeds() = 0;
    virtual void Execute(std::shared_ptr<framebuffer_kconsole> targetObject) = 0;
};

class framebuffer_kcons_cmd_print_at : public framebuffer_kcons_cmd {
private:
    std::string str;
    uint8_t col;
    uint8_t row;
public:
    framebuffer_kcons_cmd_print_at(uint8_t col, uint8_t row, const char *str) : str(str), col(col), row(row) {
    }
    ~framebuffer_kcons_cmd_print_at();
    int GetEstimatedLinefeeds() override;
    void Execute(std::shared_ptr<framebuffer_kconsole> targetObject) override;
};

framebuffer_kcons_cmd_print_at::~framebuffer_kcons_cmd_print_at() noexcept {
}

int framebuffer_kcons_cmd_print_at::GetEstimatedLinefeeds() {
    return 0;
}
void framebuffer_kcons_cmd_print_at::Execute(std::shared_ptr<framebuffer_kconsole> targetObject) {
    targetObject->print_at(col, row, str.c_str());
}

class framebuffer_kcons_cmd_printstr : public framebuffer_kcons_cmd {
private:
    std::string str;
public:
    framebuffer_kcons_cmd_printstr(const char *str) : str(str) {
    }
    ~framebuffer_kcons_cmd_printstr();
    int GetEstimatedLinefeeds() override;
    void Execute(std::shared_ptr<framebuffer_kconsole> targetObject) override;
};

framebuffer_kcons_cmd_printstr::~framebuffer_kcons_cmd_printstr() noexcept {
}

int framebuffer_kcons_cmd_printstr::GetEstimatedLinefeeds() {
    int ln{0};
    char *str = this->str.c_str();
    while (*str) {
        if (*str == '\n') {
            ++ln;
        }
        ++str;
    }
    return ln;
}

void framebuffer_kcons_cmd_printstr::Execute(std::shared_ptr<framebuffer_kconsole> targetObject) {
    (*targetObject) << str.c_str();
}

framebuffer_kcons_with_worker_thread::framebuffer_kcons_with_worker_thread(std::shared_ptr<framebuffer_kconsole> targetObject) :
    command_ring(), command_ring_extract(0), command_ring_insert(0), targetObject(targetObject), spinlock(),
    semaphore(-1), terminate(false), cursorVisible(false), worker([this] () { WorkerThread(); }),
    blink([this] () { Blink(); }) {
}

framebuffer_kcons_with_worker_thread::~framebuffer_kcons_with_worker_thread() {
    {
        critical_section cli{};
        std::lock_guard lock{spinlock};
        terminate = true;
    }
    semaphore.release();
    worker.join();
    blink.join();
    while (command_ring_extract != command_ring_insert) {
        delete command_ring[command_ring_extract];
        ++command_ring_extract;
    }
}

#define EXTRACT_MAX 128
void framebuffer_kcons_with_worker_thread::WorkerThread() {
    while (true) {
        semaphore.acquire();
        bool showCursor{false};
        unsigned int num{0};
        framebuffer_kcons_cmd *cmds[EXTRACT_MAX];
        {
            critical_section cli{};
            std::lock_guard lock{spinlock};
            if (terminate) {
                break;
            }
            while (command_ring_extract != command_ring_insert && num < EXTRACT_MAX) {
                cmds[num] = command_ring[command_ring_extract];
                ++num;
                ++command_ring_extract;
                if (command_ring_extract == RING_SIZE) {
                    command_ring_extract = 0;
                }
            }
            showCursor = cursorVisible;
        }
        {
            critical_section cli{};

            if (num > 0) {
                targetObject->SetCursorVisible(false);
            }
            unsigned int remaining{num};
            int height = (int) targetObject->GetHeight();
            do {
                int linefeeds{0};
                for (int i = 0; i < num; i++) {
                    auto estimate = cmds[i]->GetEstimatedLinefeeds();
                    if ((linefeeds + estimate) >= (height - 1) && i > 0) {
                        num = i;
                        break;
                    }
                    linefeeds += estimate;
                }
                if (linefeeds > 0) {
                    targetObject->MakeRoomForLinefeeds(linefeeds);
                }
                for (int i = 0; i < num; i++) {
                    cmds[i]->Execute(targetObject);
                    delete cmds[i];
                }
                for (unsigned int i = num; i < remaining; i++) {
                    cmds[i - num] = cmds[i];
                }
                remaining -= num;
                num = remaining;
            } while (num > 0);
            targetObject->SetCursorVisible(showCursor);
        }
    }
}

bool framebuffer_kcons_with_worker_thread::InsertCommand(framebuffer_kcons_cmd *cmd) {
    bool success{false};
    {
        critical_section cli{};
        std::lock_guard lock{spinlock};
        unsigned int next = command_ring_insert + 1;
        if (next == RING_SIZE) {
            next = 0;
        }
        if (next != command_ring_extract) {
            command_ring[command_ring_insert] = cmd;
            command_ring_insert = next;
            success = true;
        }
    }
    if (success) {
        semaphore.release();
    }
    return success;
}

void framebuffer_kcons_with_worker_thread::print_at(uint8_t col, uint8_t row, const char *str) {
    framebuffer_kcons_cmd *cmd{new framebuffer_kcons_cmd_print_at(col, row, str)};
    if (!InsertCommand(cmd)) {
        delete cmd;
    }
}

framebuffer_kcons_with_worker_thread &framebuffer_kcons_with_worker_thread::operator<<(const char *str) {
    framebuffer_kcons_cmd* cmd{new framebuffer_kcons_cmd_printstr(str)};
    if (!InsertCommand(cmd)) {
        delete cmd;
    }
    return *this;
}

void framebuffer_kcons_with_worker_thread::Blink() {
    while (true) {
        {
            critical_section cli{};
            std::lock_guard lock{spinlock};
            if (terminate) {
                break;
            }
            cursorVisible = !cursorVisible;
        }
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(500ms);
    }
}
