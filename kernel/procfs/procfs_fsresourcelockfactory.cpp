//
// Created by sigsegv on 11/8/23.
//

#include <concurrency/hw_spinlock.h>
#include "procfs_fsresourcelockfactory.h"

class procfs_fsresourcelock : public fsresourcelock {
private:
    hw_spinlock mtx{};
public:
    void lock() override;
    void unlock() override;
};

void procfs_fsresourcelock::lock() {
    mtx.lock();
}

void procfs_fsresourcelock::unlock() {
    mtx.unlock();
}

std::shared_ptr<fsresourcelock> procfs_fsresourcelockfactory::Create() {
    return std::make_shared<procfs_fsresourcelock>();
}
