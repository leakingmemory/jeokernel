//
// Created by sigsegv on 11/15/22.
//

#include <procfs/procfs.h>
#include <filesystems/filesystem.h>
#include <exec/procthread.h>
#include <files/symlink.h>
#include <files/fsresource.h>
#include <sstream>
#include <core/uname_info.h>
#include "ProcAuxv.h"
#include "ProcStrfile.h"
#include "ProcProcessStat.h"
#include "ProcUptime.h"
#include "ProcMeminfo.h"
#include "ProcMounts.h"
#include "procfs_fsresourcelockfactory.h"

class procfs_symlink : public symlink, public fsresource<procfs_symlink> {
private:
    std::string link{};
    procfs_symlink(fsresourcelockfactory &lockfactory, const std::string &link) : fsresource<procfs_symlink>(lockfactory), link(link) {}
public:
    static std::shared_ptr<procfs_symlink> Create(const std::string &link);
    procfs_symlink * GetResource() override;
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
    std::string GetLink() override;
};

std::shared_ptr<procfs_symlink> procfs_symlink::Create(const std::string &link) {
    procfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<procfs_symlink> symlink{new procfs_symlink(lockfactory, link)};
    symlink->SetSelfRef(symlink);
    return symlink;
}

procfs_symlink *procfs_symlink::GetResource() {
    return this;
}

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

class procfs_directory : public directory, public fsresource<procfs_directory> {
protected:
    explicit procfs_directory(fsresourcelockfactory &lockfactory) : fsresource<procfs_directory>(lockfactory) {}
public:
    procfs_directory() = delete;
    procfs_directory * GetResource() override;
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
    entries_result Entries() override = 0;
};

procfs_directory *procfs_directory::GetResource() {
    return this;
}

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

class procfs_kerneldir : public procfs_directory {
private:
    procfs_kerneldir(fsresourcelockfactory &lockfactory) : procfs_directory(lockfactory) {}
public:
    static std::shared_ptr<procfs_kerneldir> Create();
    entries_result Entries() override;
};

std::shared_ptr<procfs_kerneldir> procfs_kerneldir::Create() {
    procfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<procfs_kerneldir> kerneldir{new procfs_kerneldir(lockfactory)};
    kerneldir->SetSelfRef(kerneldir);
    return kerneldir;
}

class procfs_directory_entry : public directory_entry {
private:
    std::function<directory_entry_result (const std::shared_ptr<fsreferrer> &referrer)> load;
public:
    procfs_directory_entry(const std::string &name, const std::function<directory_entry_result (const std::shared_ptr<fsreferrer> &referrer)> &load) : directory_entry(name), load(load) {}
    directory_entry_result LoadItem(const std::shared_ptr<fsreferrer> &referrer) override;
};

directory_entry_result procfs_directory_entry::LoadItem(const std::shared_ptr<fsreferrer> &referrer) {
    return load(referrer);
}

entries_result procfs_kerneldir::Entries() {
    auto uname = get_uname_info();
    std::string osrelease{uname.linux_level};
    osrelease.append("-");
    osrelease.append(uname.variant);
    osrelease.append("-");
    osrelease.append(uname.arch);
    entries_result result{.entries = {}, .status = fileitem_status::SUCCESS};
    {
        result.entries.push_back(
                std::make_shared<procfs_directory_entry>("osrelease", [osrelease] (const std::shared_ptr<fsreferrer> &referrer) {
                    auto file = ProcStrfile::Create(osrelease);
                    file->SetMode(00444);
                    directory_entry_result result{.file = file->CreateReference(referrer), .status = fileitem_status::SUCCESS};
                    return result;
                }));
    }
    {
        std::string maxPidStr{};
        {
            std::stringstream strs{};
            strs << std::dec << Process::GetMaxPid();
            maxPidStr = strs.str();
        }
        result.entries.push_back(std::make_shared<procfs_directory_entry>("pid_max", [maxPidStr] (const std::shared_ptr<fsreferrer> &referrer) {
            auto file = ProcStrfile::Create(maxPidStr);
            file->SetMode(00444);
            fsreference<ProcDatafile> refProc = file->CreateReference(referrer);
            fsreference<fileitem> fileItemRef = fsreference_dynamic_cast<fileitem>(std::move(refProc));
            directory_entry_result result{.file = std::move(fileItemRef), .status = fileitem_status::SUCCESS};
            return result;
        }));
    }
    return result;
}

class procfs_sysdir : public procfs_directory {
private:
    procfs_sysdir(fsresourcelockfactory &lockfactory) : procfs_directory(lockfactory) {}
public:
    procfs_sysdir() = delete;
    static std::shared_ptr<procfs_sysdir> Create();
    entries_result Entries() override;
};

std::shared_ptr<procfs_sysdir> procfs_sysdir::Create() {
    procfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<procfs_sysdir> sysdir{new procfs_sysdir(lockfactory)};
    sysdir->SetSelfRef(sysdir);
    return sysdir;
}

entries_result procfs_sysdir::Entries() {
    entries_result result{.entries = {}, .status = fileitem_status::SUCCESS};
    result.entries.push_back(std::make_shared<procfs_directory_entry>("kernel", [] (const std::shared_ptr<fsreferrer> &referrer) {
        auto file = procfs_kerneldir::Create();
        auto procRef = file->CreateReference(referrer);
        fsreference<fileitem> fileRef = fsreference_dynamic_cast<fileitem>(std::move(procRef));
        directory_entry_result result{.file = std::move(fileRef), .status = fileitem_status::SUCCESS};
        return result;
    }));
    return result;
}

class procfs_procdir : public procfs_directory {
private:
    std::weak_ptr<Process> w_process;
    procfs_procdir(fsresourcelockfactory &lockfactory, const std::weak_ptr<Process> &process) : procfs_directory(lockfactory), w_process(process) {}
public:
    static std::shared_ptr<procfs_procdir> Create(const std::weak_ptr<Process> &process);
    pid_t getpid();
    entries_result Entries() override;
};

std::shared_ptr<procfs_procdir> procfs_procdir::Create(const std::weak_ptr<Process> &process) {
    procfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<procfs_procdir> procdir{new procfs_procdir(lockfactory, process)};
    procdir->SetSelfRef(procdir);
    return procdir;
}

pid_t procfs_procdir::getpid() {
    auto process = w_process.lock();
    if (!process) {
        return -1;
    }
    return process->getpid();
}

entries_result procfs_procdir::Entries() {
    auto w_process = this->w_process;
    auto process = w_process.lock();
    entries_result result{.entries = {}, .status = fileitem_status::SUCCESS};
    if (process) {
        result.entries.emplace_back(std::make_shared<procfs_directory_entry>("auxv", [w_process](const std::shared_ptr<fsreferrer> &referrer) mutable {
            auto process = w_process.lock();
            if (!process) {
                directory_entry_result result{.file = {}, .status = fileitem_status::IO_ERROR};
                return result;
            }
            auto file = ProcAuxv::Create(process->GetAuxv());
            auto procRef = file->CreateReference(referrer);
            fsreference<fileitem> fileRef = fsreference_dynamic_cast<fileitem>(std::move(procRef));
            directory_entry_result result{.file = std::move(fileRef), .status = fileitem_status::SUCCESS};
            return result;
        }));
        result.entries.emplace_back(std::make_shared<procfs_directory_entry>("stat", [w_process](const std::shared_ptr<fsreferrer> &referrer) mutable {
            auto process = w_process.lock();
            if (!process) {
                directory_entry_result result{.file = {}, .status = fileitem_status::IO_ERROR};
                return result;
            }
            auto file = ProcProcessStat::Create(process);
            auto procRef = file->CreateReference(referrer);
            fsreference<fileitem> fileRef = fsreference_dynamic_cast<fileitem>(std::move(procRef));
            directory_entry_result result{.file = std::move(fileRef), .status = fileitem_status::SUCCESS};
            return result;
        }));
    }
    return result;
}

class procfs_root : public procfs_directory {
private:
    procfs_root(fsresourcelockfactory &lockfactory) : procfs_directory(lockfactory) {}
public:
    procfs_root() = delete;
    static std::shared_ptr<procfs_root> Create();
    entries_result Entries() override;
};

std::shared_ptr<procfs_root> procfs_root::Create() {
    procfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<procfs_root> root{new procfs_root(lockfactory)};
    root->SetSelfRef(root);
    return root;
}

template <typename T> concept ProcStatelessDirectoryCreate = requires(std::shared_ptr<T> t)
{
    { t = T::Create() };
};
template <typename T> concept ProcStatelessDirectoryReference = requires(fsreference<T> ref)
{
    { ref = std::declval<std::shared_ptr<T>>()->CreateReference(std::declval<std::shared_ptr<fsreferrer>>()) };
};
template <typename T> concept ProcStatelessDirectory = ProcStatelessDirectoryCreate<T> && ProcStatelessDirectoryReference<T>;

template <ProcStatelessDirectory T> fsreference<fileitem> CreateProcfsFileitemRef(const std::shared_ptr<fsreferrer> &referrer) {
    auto file = T::Create();
    auto genRef = file->CreateReference(referrer);
    fsreference<fileitem> fileRef = fsreference_dynamic_cast<fileitem>(std::move(genRef));
    return fileRef;
}
template <ProcStatelessDirectory T> std::shared_ptr<directory_entry> procfs_stateless_directory(const std::string &name) {
    return std::make_shared<procfs_directory_entry>(name, [] (const std::shared_ptr<fsreferrer> &referrer) {
        fsreference<fileitem> fileRef = CreateProcfsFileitemRef<T>(referrer);
        directory_entry_result result{.file = std::move(fileRef), .status = fileitem_status::SUCCESS};
        return result;
    });
}

entries_result procfs_root::Entries() {
    entries_result result{.entries = {}, .status = fileitem_status::SUCCESS};
    result.entries.push_back(procfs_stateless_directory<procfs_sysdir>("sys"));
    result.entries.push_back(procfs_stateless_directory<ProcUptime>("uptime"));
    result.entries.push_back(procfs_stateless_directory<ProcMeminfo>("meminfo"));
    result.entries.push_back(procfs_stateless_directory<ProcMounts>("mounts"));
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
            for (const auto &proc_cref: procs) {
                std::weak_ptr<Process> proc = proc_cref;
                std::string name{};
                {
                    std::stringstream ss{};
                    ss << std::dec << proc_cref->getpid();
                    name = ss.str();
                }
                std::shared_ptr<directory_entry> dirent = std::make_shared<procfs_directory_entry>(
                        name,
                        [proc] (const std::shared_ptr<fsreferrer> &referrer) {
                            auto file = procfs_procdir::Create(proc);
                            auto genRef = file->CreateReference(referrer);
                            fsreference<fileitem> fileRef = fsreference_dynamic_cast<fileitem>(std::move(genRef));
                            directory_entry_result result{.file = std::move(fileRef), .status = fileitem_status::SUCCESS};
                            return result;
                        }
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
        std::shared_ptr<directory_entry> dirent = std::make_shared<procfs_directory_entry>(
                "self",
                [self_link] (const std::shared_ptr<fsreferrer> &referrer) {
                        auto file = procfs_symlink::Create(self_link);
                        auto genRef = file->CreateReference(referrer);
                        fsreference<fileitem> fileRef = fsreference_dynamic_cast<fileitem>(std::move(genRef));
                        directory_entry_result result{.file = std::move(fileRef), .status = fileitem_status::SUCCESS};
                        return result;
                }
        );
        result.entries.push_back(dirent);
    }
    return result;
}

class procfs : public filesystem {
public:
    filesystem_get_node_result<fsreference<directory>> GetRootDirectory(std::shared_ptr<filesystem> shared_this, const std::shared_ptr<fsreferrer> &referrer) override;
};

filesystem_get_node_result<fsreference<directory>> procfs::GetRootDirectory(std::shared_ptr<filesystem> shared_this, const std::shared_ptr<fsreferrer> &referrer) {
    auto root = procfs_root::Create();
    auto rootDirRef = root->CreateReference(referrer);
    auto dirRef = fsreference_dynamic_cast<directory>(std::move(rootDirRef));
    return {.node = std::move(dirRef), .status = filesystem_status::SUCCESS};
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