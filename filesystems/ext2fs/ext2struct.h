//
// Created by sigsegv on 4/25/22.
//

#ifndef FSBITS_EXT2STRUCT_H
#define FSBITS_EXT2STRUCT_H

#include <filesystems/endian.h>

struct ext2super {
    little_endian<uint32_t> total_inodes;
    little_endian<uint32_t> total_blocks;
    little_endian<uint32_t> reserved_blocks;
    little_endian<uint32_t> unallocated_blocks;
    little_endian<uint32_t> unallocated_inodes;
    little_endian<uint32_t> block_superblock;
    little_endian<uint32_t> log2_blocksize_minus_10;
    little_endian<uint32_t> log2_fragsize_minus_10;
    little_endian<uint32_t> blocks_per_group;
    little_endian<uint32_t> frags_per_group;
    little_endian<uint32_t> inodes_per_group;
    little_endian<uint32_t> last_mount;
    little_endian<uint32_t> last_write;
    little_endian<uint16_t> mounts_since_check;
    little_endian<uint16_t> mounts_to_require_check;
    little_endian<uint16_t> ext2_signature;
    little_endian<uint16_t> fs_state;
    little_endian<uint16_t> fs_error_recovery;
    little_endian<uint16_t> minor_version;
    little_endian<uint32_t> time_of_last_check;
    little_endian<uint32_t> check_interval;
    little_endian<uint32_t> os_id;
    little_endian<uint32_t> major_version;
    little_endian<uint16_t> owner_uid;
    little_endian<uint16_t> owner_gid;
};

#endif //FSBITS_EXT2STRUCT_H
