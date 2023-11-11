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

resolve_return_value
Mount::DoMount(SyscallCtx &ctx, const std::string &i_dev, const std::string &dir, const std::string &i_type,
               unsigned long flags, const std::string &opts) {
    std::string dev{i_dev};
    std::string type{i_type};
    std::cout << "mount(" << dev << ", " << dir << ", " << type << ", " << std::hex << flags << std::dec << ", "
              << opts << ")\n";
    auto mountpointResult = ctx.GetProcess().ResolveFile(dir);
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
    std::shared_ptr<kdirectory> mountpoint = std::dynamic_pointer_cast<kdirectory>(mountpointResult.result);
    if (!mountpoint) {
        return ctx.Return(-ENOTDIR);
    }

    auto deviceResult = ctx.GetProcess().ResolveFile(dev);
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

    Queue(task_id, [ctx, openfs, mountpoint, dev, type] () mutable {
        auto fs = openfs();
        if (!fs) {
            ctx.ReturnWhenNotRunning(-EINVAL);
            return;
        }
        mountpoint->Mount(dev, type, "ro", fs);
        ctx.ReturnWhenNotRunning(0);
    });
    return ctx.Async();
}

int64_t Mount::Call(int64_t uptr_dev, int64_t uptr_dir, int64_t uptr_type, int64_t u_flags, SyscallAdditionalParams &params) {
    int64_t uptr_data = params.Param5();
    SyscallCtx ctx{params};
    return ctx.ReadString(uptr_dev, [this, ctx, uptr_dir, uptr_type, u_flags, uptr_data] (const std::string &i_dev) mutable {
        std::string dev{i_dev};
        return ctx.NestedReadString(uptr_dir, [this, ctx, dev, uptr_type, u_flags, uptr_data] (const std::string &i_dir) mutable {
            std::string dir{i_dir};
            return ctx.NestedReadString(uptr_type, [this, ctx, dev, dir, u_flags, uptr_data] (const std::string &i_type) mutable {
                if (uptr_data != 0) {
                    std::string type{i_type};
                    return ctx.NestedReadString(uptr_data, [this, ctx, dev, dir, type, u_flags] (const std::string &data) mutable {
                        return DoMount(ctx, dev, dir, type, (unsigned long) u_flags, data);
                    });
                }
                return DoMount(ctx, dev, dir, i_type, (unsigned long) u_flags, "");
            });
        });
    });
}