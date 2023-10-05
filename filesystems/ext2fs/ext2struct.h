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
    /* dynamic rev: */
    little_endian<uint32_t> first_inode;
    little_endian<uint16_t> inode_size;
    little_endian<uint16_t> block_group_nr;
    little_endian<uint32_t> feature_compat;
    little_endian<uint32_t> feature_incompat;
    little_endian<uint32_t> feature_ro_compat;
    little_endian<uint64_t> uuid_low;
    little_endian<uint64_t> uuid_high;
    char volume_name[16];
    char last_mounted[64];
    little_endian<uint32_t> algo_bitmap;
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
    bool &dirty;
    uint32_t bit;
public:
    ext2bitmap_bit(little_endian<uint32_t> &entry, bool &dirty, uint32_t bit) : entry(entry), dirty(dirty), bit(bit) {
    }
    operator bool() {
        uint32_t val = entry;
        return (val & (((typeof(val)) 1) << bit)) != 0;
    }
    ext2bitmap_bit &operator =(bool value) {
        if (value) {
            uint32_t prev = entry;
            entry = prev | (((typeof(prev)) 1) << bit);
            if (prev != entry) {
                dirty = true;
            }
        } else {
            uint32_t prev = entry;
            entry = prev & ~(((typeof(prev)) 1) << bit);
        }
        return *this;
    }
};

class ext2bitmap {
private:
    little_endian<uint32_t> *bitmap;
    bool *dirty;
    std::size_t n, sz, blocks, blocksize;
public:
    ext2bitmap(std::size_t n, std::size_t blocksize) : bitmap(nullptr), n(n), sz(0), blocks(0), blocksize(blocksize) {
        sz = n / sizeof(uint32_t);
        blocks = sz / blocksize;
        if ((sz % blocksize) != 0) {
            ++blocks;
        }
        if ((n % sizeof(uint32_t)) != 0) {
            ++sz;
        }
        bitmap = (little_endian<uint32_t> *) malloc(sz * sizeof(uint32_t));
        dirty = (bool *) malloc(blocks * sizeof(*dirty));
    }
    ext2bitmap(const ext2bitmap &) = delete;
    ext2bitmap(ext2bitmap &&) = delete;
    ext2bitmap &operator =(const ext2bitmap &) = delete;
    ext2bitmap &operator =(ext2bitmap &&) = delete;
    ~ext2bitmap() {
        free(dirty);
        dirty = nullptr;
        free(bitmap);
        bitmap = nullptr;
    }
    ext2bitmap_bit operator[] (std::size_t i) const {
        auto bit = i & 31;
        i = i >> 5;
        auto block = i / blocksize;
        ext2bitmap_bit ref{bitmap[i], dirty[block], (uint32_t) bit};
        return ref;
    }
    std::size_t FindFree(int after = 0) const {
        for (std::size_t i = after; i < sz; i++) {
            typeof(bitmap[i]) bits = bitmap[i];
            constexpr typeof(bits) allOne = ~((typeof(bits)) 0);
            if (bits != allOne) {
                for (int b = 0; b < 32; b++) {
                    uint32_t bitmask = ((uint32_t) 1) << b;
                    if ((bits & bitmask) == 0) {
                        std::size_t inodenum = i * 32;
                        inodenum += b;
                        if (inodenum < n) {
                            return inodenum + 1;
                        } else {
                            return 0;
                        }
                    }
                }
            }
        }
        return 0;
    }
    void *Pointer() {
        return bitmap;
    }
    [[nodiscard]] std::vector<uint32_t> DirtyBlocks() const {
        std::vector<uint32_t> result{};
        for (std::remove_const<typeof(blocks)>::type i = 0; i < blocks; i++) {
            if (dirty[i]) {
                result.push_back(i);
            }
        }
        return result;
    }
    const void *PointerToBlock(std::size_t blk) const {
        uintptr_t offset = blk;
        offset = offset * blocksize;
        return ((uint8_t *) bitmap) + offset;
    }
};

#define EXT2_NUM_DIRECT_BLOCK_PTRS  12
#define EXT2_NUM_BLOCK_POINTERS     (EXT2_NUM_DIRECT_BLOCK_PTRS + 3)

struct ext2inode {
    little_endian<uint16_t> mode;
    little_endian<uint16_t> uid;
    little_endian<uint32_t> size;
    little_endian<uint32_t> atime;
    little_endian<uint32_t> ctime;
    little_endian<uint32_t> mtime;
    little_endian<uint32_t> dtime;
    little_endian<uint16_t> gid;
    little_endian<uint16_t> links_count;
    little_endian<uint32_t> blocks;
    little_endian<uint32_t> flags;
    uint32_t reserved1; // Hurd?
    little_endian<uint32_t> block[EXT2_NUM_BLOCK_POINTERS];
    little_endian<uint32_t> generation;
    little_endian<uint32_t> file_acl;
    little_endian<uint32_t> dir_acl;
    little_endian<uint32_t> fragment_address;
    uint8_t fragment_number;
    uint8_t fragment_size;
    uint16_t pad1;
    little_endian<uint16_t> uid_high;
    little_endian<uint16_t> gid_high;
    uint32_t extspace[1];
};
static_assert(sizeof(ext2inode) == 128);

constexpr uint8_t Ext2FileType_Regular = 1;
constexpr uint8_t Ext2FileType_Directory = 2;
constexpr uint8_t Ext2FileType_Symlink = 7;

struct ext2dirent {
    little_endian<uint32_t> inode;
    little_endian<uint16_t> rec_len;
    uint8_t name_len;
    uint8_t file_type;

private:
    const char *NamePtr() {
        return ((const char *) this) + sizeof(*this);
    }
public:
    std::string Name() {
        std::string name{};
        name.append(NamePtr(), name_len);
        return name;
    }
} __attribute__((__packed__));
static_assert(sizeof(ext2dirent) == 8);

#endif //FSBITS_EXT2STRUCT_H
