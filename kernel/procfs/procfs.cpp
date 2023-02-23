//
// Created by sigsegv on 11/15/22.
//

#include <procfs/procfs.h>
#include <filesystems/filesystem.h>
#include <exec/procthread.h>
#include <files/symlink.h>
#include <sstream>

class procfs_symlink : public symlink {
private:
    std::string link{};
public:
    procfs_symlink(const std::string &link) : link(link) {}
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
    std::string GetLink() override;
};

uint32_t procfs_symlink::Mode() {
    return 00120777;
}

std::size_t procfs_symlink::Size() {
    return link.size();
}

uintptr_t procfs_symlink::InodeNum() {
    return 0;
}

uint32_t procfs_symlink::BlockSize() {
    return 0;
}

uintptr_t procfs_symlink::SysDevId() {
    return 0;
}

file_getpage_result procfs_symlink::GetPage(std::size_t pagenum) {
    return {.page = {}, .status = fileitem_status::INVALID_REQUEST};
}

file_read_result procfs_symlink::Read(uint64_t offset, void *ptr, std::size_t length) {
    return {.size = 0, .status = fileitem_status::INVALID_REQUEST};
}

std::string procfs_symlink::GetLink() {
    return link;
}

class procfs_directory : public directory {
public:
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
    entries_result Entries() override = 0;
};

uint32_t procfs_directory::Mode() {
    return 0040555;
}

std::size_t procfs_directory::Size() {
    return 0;
}

uintptr_t procfs_directory::InodeNum() {
    return 0;
}

uint32_t procfs_directory::BlockSize() {
    return 0;
}

uintptr_t procfs_directory::SysDevId() {
    return 0;
}

file_getpage_result procfs_directory::GetPage(std::size_t pagenum) {
    return {.page = {}, .status = fileitem_status::INVALID_REQUEST};
}

file_read_result procfs_directory::Read(uint64_t offset, void *ptr, std::size_t length) {
    return {.size = 0, .status = fileitem_status::INVALID_REQUEST};
}

class procfs_procdir : public procfs_directory {
private:
    pid_t pid;
public:
    explicit procfs_procdir(pid_t pid) : pid(pid) {}
    pid_t getpid() { return pid; };
    entries_result Entries() override;
};

entries_result procfs_procdir::Entries() {
    return {.entries = {}, .status = fileitem_status::SUCCESS};
}

class procfs_root : public procfs_directory {
public:
    entries_result Entries() override;
};

entries_result procfs_root::Entries() {
    entries_result result{.entries = {}, .status = fileitem_status::SUCCESS};
    pid_t self{0};
    {
        auto *scheduler = get_scheduler();
        auto task_id = scheduler->get_working_for_task_id();
        if (task_id == 0) {
            task_id = scheduler->get_current_task_id();
        }
        {
            std::vector<std::shared_ptr<Process>> procs{};
            scheduler->all_tasks([&procs, &self, task_id](task &t) {
                auto *procthread = t.get_resource<ProcThread>();
                if (procthread != nullptr) {
                    pid_t pid = procthread->getpid();
                    if (t.get_id() == task_id) {
                        self = pid;
                    }
                    bool found{false};
                    for (auto &entry: procs) {
                        if (entry->getpid() == pid) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        procs.push_back(procthread->GetProcess());
                    }
                }
            });
            for (const auto &proc: procs) {
                std::string name{};
                {
                    std::stringstream ss{};
                    ss << std::dec << proc->getpid();
                    name = ss.str();
                }
                std::shared_ptr<directory_entry> dirent = std::make_shared<directory_entry>(
                        name,
                        std::make_shared<procfs_procdir>(proc->getpid())
                );
                result.entries.push_back(dirent);
            }
        }
    }
    if (self != 0) {
        std::string self_link{};
        {
            std::stringstream ss{};
            ss << self;
            self_link = ss.str();
        }
        std::shared_ptr<directory_entry> dirent = std::make_shared<directory_entry>(
                "self",
                std::make_shared<procfs_symlink>(self_link)
        );
        result.entries.push_back(dirent);
    }
    return result;
}

class procfs : public filesystem {
public:
    filesystem_get_node_result<directory> GetRootDirectory(std::shared_ptr<filesystem> shared_this) override;
};

filesystem_get_node_result<directory> procfs::GetRootDirectory(std::shared_ptr<filesystem> shared_this) {
    return {.node = std::make_shared<procfs_root>(), .status = filesystem_status::SUCCESS};
}

class procfs_provider : public special_filesystem_provider {
public:
    std::string name() const override;
    std::shared_ptr<filesystem> open() const override;
};

std::string procfs_provider::name() const {
    return "procfs";
}

std::shared_ptr<filesystem> procfs_provider::open() const {
    return std::make_shared<procfs>();
}

void InstallProcfs() {
    add_filesystem_provider(std::make_shared<procfs_provider>());
}