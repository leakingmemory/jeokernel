//
// Created by sigsegv on 3/8/23.
//

#include "Mount.h"
#include "SyscallCtx.h"
#include <core/blockdevsystem.h>
#include <core/blockdev_devfs_node.h>
#include <iostream>
#include <errno.h>
#include <exec/procthread.h>
#include <kfs/blockdev_writer.h>
#include <resource/reference.h>
#include <resource/referrer.h>


class Mount_Call : public referrer {
private:
    std::weak_ptr<Mount_Call> selfRef{};
    Mount_Call() : referrer("Mount_Call") {}
public:
    static std::shared_ptr<Mount_Call> Create();
    std::string GetReferrerIdentifier() override;
    resolve_return_value Call(SyscallCtx &ctx, const std::string &i_dev, const std::string &dir, const std::string &i_type,
                              unsigned long flags, const std::string &opts);
};

std::shared_ptr<Mount_Call> Mount_Call::Create() {
    std::shared_ptr<Mount_Call> mountCall{new Mount_Call()};
    std::weak_ptr<Mount_Call> weakPtr{mountCall};
    mountCall->selfRef = weakPtr;
    return mountCall;
}

std::string Mount_Call::GetReferrerIdentifier() {
    return "";
}

resolve_return_value
Mount_Call::Call(SyscallCtx &ctx, const std::string &i_dev, const std::string &dir, const std::string &i_type,
                 unsigned long flags, const std::string &opts) {
    std::string dev{i_dev};
    std::string type{i_type};
    std::cout << "mount(" << dev << ", " << dir << ", " << type << ", " << std::hex << flags << std::dec << ", "
              << opts << ")\n";
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto mountpointResult = ctx.GetProcess().ResolveFile(selfRef, dir);
    if (mountpointResult.status != kfile_status::SUCCESS) {
        switch (mountpointResult.status) {
            case kfile_status::NOT_DIRECTORY:
                return ctx.Return(-ENOTDIR);
            case kfile_status::IO_ERROR:
            default:
                return ctx.Return(-EIO);
        }
    }
    if (!mountpointResult.result) {
        return ctx.Return(-ENOENT);
    }
    std::shared_ptr<reference<kdirectory>> mountpoint = std::make_shared<reference<kdirectory>>();
    *mountpoint = reference_dynamic_cast<kdirectory>(std::move(mountpointResult.result));
    if (!(*mountpoint)) {
        return ctx.Return(-ENOTDIR);
    }

    auto deviceResult = ctx.GetProcess().ResolveFile(selfRef, dev);
    if (deviceResult.status != kfile_status::SUCCESS) {
        switch (deviceResult.status) {
            case kfile_status::NOT_DIRECTORY:
                return ctx.Return(-ENOTDIR);
            case kfile_status::IO_ERROR:
            default:
                return ctx.Return(-EIO);
        }
    }

    std::function<std::shared_ptr<filesystem> ()> openfs{};

    if (deviceResult.result) {
        auto node = blockdev_devfs_node::FromFileItem(deviceResult.result->GetFileitem());
        if (node) {
            auto blockdev = node->GetBlockdev();
            if (!blockdev) {
                return ctx.Return(-EINVAL);
            }
            openfs = [ctx, dev, type, blockdev] () {
                auto fs = get_blockdevsystem().OpenFilesystem(type, blockdev);
                if (fs) {
                    blockdev_writer::GetInstance().OpenForWrite(fs);
                } else {
                    std::cerr << "OpenFilesystem failed for " << type << " on " << dev << "\n";
                }
                return fs;
            };
        }
    }
    if (!openfs) {
        openfs = [type] () {
            return get_blockdevsystem().OpenFilesystem(type);
        };
    }

    auto task_id = get_scheduler()->get_current_task_id();

    ctx.GetProcess().QueueBlocking(task_id, [ctx, openfs, mountpoint, dev, type] () mutable {
        auto fs = openfs();
        if (!fs) {
            ctx.ReturnWhenNotRunning(-EINVAL);
            return;
        }
        (*mountpoint)->Mount(dev, type, "ro", fs);
        ctx.ReturnWhenNotRunning(0);
    });
    return ctx.Async();
}

int64_t Mount::Call(int64_t uptr_dev, int64_t uptr_dir, int64_t uptr_type, int64_t u_flags, SyscallAdditionalParams &params) {
    int64_t uptr_data = params.Param5();
    SyscallCtx ctx{params, "Mount"};
    return ctx.ReadString(uptr_dev, [this, ctx, uptr_dir, uptr_type, u_flags, uptr_data] (const std::string &i_dev) mutable {
        std::string dev{i_dev};
        return ctx.NestedReadString(uptr_dir, [this, ctx, dev, uptr_type, u_flags, uptr_data] (const std::string &i_dir) mutable {
            std::string dir{i_dir};
            return ctx.NestedReadString(uptr_type, [this, ctx, dev, dir, u_flags, uptr_data] (const std::string &i_type) mutable {
                if (uptr_data != 0) {
                    std::string type{i_type};
                    return ctx.NestedReadString(uptr_data, [this, ctx, dev, dir, type, u_flags] (const std::string &data) mutable {
                        return Mount_Call::Create()->Call(ctx, dev, dir, type, (unsigned long) u_flags, data);
                    });
                }
                return Mount_Call::Create()->Call(ctx, dev, dir, i_type, (unsigned long) u_flags, "");
            });
        });
    });
}