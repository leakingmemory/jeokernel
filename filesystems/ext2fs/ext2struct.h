//
// Created by sigsegv on 4/25/22.
//

#ifndef FSBITS_EXT2STRUCT_H
#define FSBITS_EXT2STRUCT_H

#include <filesystems/endian.h>

#define EXT2_SIGNATURE 0xEF53

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

struct ext2blockgroup {
    little_endian<uint32_t> block_bitmap;
    little_endian<uint32_t> inode_bitmap;
    little_endian<uint32_t> inode_table;
    little_endian<uint16_t> free_blocks_count;
    little_endian<uint16_t> free_inodes_count;
    little_endian<uint16_t> used_dirs_count;
    little_endian<uint16_t> pad;
    little_endian<uint32_t> reserved[3];
};

class ext2blockgroups {
private:
    ext2blockgroup *groups;
    std::size_t n;
public:
    ext2blockgroups(std::size_t n) : groups(nullptr), n(n) {
        groups = (ext2blockgroup *) malloc(sizeof(*groups) * n);
    }
    ext2blockgroups(const ext2blockgroups &) = delete;
    ext2blockgroups(ext2blockgroups &&) = delete;
    ext2blockgroups &operator =(const ext2blockgroups &) = delete;
    ext2blockgroups &operator =(ext2blockgroups &&) = delete;
    ~ext2blockgroups() {
        free(groups);
        groups = nullptr;
    }
    std::size_t Size() {
        return n;
    }
    ext2blockgroup &operator [] (std::size_t i) {
        return groups[i];
    }
};

class ext2bitmap_bit {
private:
    little_endian<uint32_t> &entry;
    uint32_t bit;
public:
    ext2bitmap_bit(little_endian<uint32_t> &entry, uint32_t bit) : entry(entry), bit(bit) {
    }
    operator bool() {
        uint32_t val = entry;
        return (val & bit) != 0;
    }
    ext2bitmap_bit &operator =(bool value) {
        if (value) {
            uint32_t prev = entry;
            entry = prev | bit;
        } else {
            uint32_t prev = entry;
            entry = prev & ~bit;
        }
        return *this;
    }
};

class ext2bitmap {
private:
    little_endian<uint32_t> *bitmap;
    std::size_t n;
public:
    ext2bitmap(std::size_t n) : bitmap(nullptr), n(n) {
        auto sz = n / sizeof(uint32_t);
        if ((n % sizeof(uint32_t)) != 0) {
            ++sz;
        }
        bitmap = (little_endian<uint32_t> *) malloc(sz * sizeof(uint32_t));
    }
    ext2bitmap(const ext2bitmap &) = delete;
    ext2bitmap(ext2bitmap &&) = delete;
    ext2bitmap &operator =(const ext2bitmap &) = delete;
    ext2bitmap &operator =(ext2bitmap &&) = delete;
    ~ext2bitmap() {
        free(bitmap);
        bitmap = nullptr;
    }
    ext2bitmap_bit operator[] (std::size_t i) {
        auto bit = i & 31;
        i = i >> 5;
        ext2bitmap_bit ref{bitmap[i], (uint32_t) bit};
        return ref;
    }
    void *Pointer() {
        return bitmap;
    }
};

#endif //FSBITS_EXT2STRUCT_H
