//
// Created by sigsegv on 3/30/22.
//

#include <core/blockdevsystem.h>
#include <mutex>

void blockdev_interface::Stop() {
    stop = true;
    semaphore.release();
}

void blockdev_interface::SetBlocksize(std::size_t size) {
    blocksize = size;
}

size_t blockdev_interface::GetBlocksize() {
    return blocksize;
}

void blockdev_interface::SetNumBlocks(std::size_t numBlocks) {
    this->numBlocks = numBlocks;
}

size_t blockdev_interface::GetNumBlocks() {
    return numBlocks;
}

std::shared_ptr<blockdev_command> blockdev_interface::NextCommand() {
    semaphore.acquire();
    std::lock_guard lock{_lock};
    if (stop) {
        return {};
    }
    auto iterator = queue.begin();
    std::shared_ptr<blockdev_command> cmd = *iterator;
    queue.erase(iterator);
    return cmd;
}

void blockdev_interface::Submit(std::shared_ptr<blockdev_command> command) {
    queue.push_back(command);
    semaphore.release();
}
