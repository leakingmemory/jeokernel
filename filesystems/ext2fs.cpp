//
// Created by sigsegv on 4/25/22.
//

#include "ext2fs.h"
#include "ext2fs_inode.h"
#include "ext2fs_inode_reader.h"
#include <blockdevs/blockdev.h>
#include <cstring>
#include <strings.h>
#include <iostream>
#include <files/symlink.h>
#include "ext2fs/ext2struct.h"

//#define DEBUG_INODE
//#define DEBUG_DIR

ext2fs::ext2fs(std::shared_ptr<blockdev> bdev) : blockdev_filesystem(bdev), mtx(), superblock(), groups(), inodes(), BlockSize(0) {
    sys_dev_id = bdev->GetDevId();
    auto blocksize = bdev->GetBlocksize();
    {
        std::shared_ptr<blockdev_block> blocks;
        std::size_t superblock_offset;
        {
            auto superblock_start = 1024 / blocksize;
            {
                auto start_offset = superblock_start * blocksize;
                superblock_offset = (std::size_t) (1024 - start_offset);
            }
            auto superblock_size = (2048 / blocksize) - superblock_start;
            if (superblock_size < 1) {
                superblock_size = 1;
            }
            blocks = bdev->ReadBlock(superblock_start, superblock_size);
        }
        superblock = std::make_unique<ext2super>();
        memmove(&(*superblock), ((uint8_t *) blocks->Pointer()) + superblock_offset, sizeof(*superblock));
        BlockSize = 1024 << superblock->log2_blocksize_minus_10;
    }
}

bool ext2fs::HasSuperblock() const {
    if (superblock) {
        return true;
    } else {
        return false;
    }
}

int ext2fs::VersionMajor() const {
    return superblock->major_version;
}

int ext2fs::VersionMinor() const {
    return superblock->minor_version;
}

uint16_t ext2fs::FsSignature() const {
    return superblock->ext2_signature;
}

std::size_t ext2fs::InodeSize() const {
    if (IsDynamic()) {
        return superblock->inode_size;
    } else {
        return 128;
    }
}

uint64_t ext2fs::FsBlockToPhysBlock(uint64_t fs_block) {
    fs_block *= BlockSize;
    return fs_block / bdev->GetBlocksize();
}

uint64_t ext2fs::FsBlockOffsetOnPhys(uint64_t fs_block) {
    fs_block *= BlockSize;
    return fs_block % bdev->GetBlocksize();
}

uint64_t ext2fs::FsBlocksToPhysBlocks(uint64_t fs_block, uint64_t fs_len) {
    auto start = FsBlockToPhysBlock(fs_block);
    auto fs_end = fs_block + fs_len;
    auto end = FsBlockToPhysBlock(fs_end);
    if (FsBlockOffsetOnPhys(fs_end) != 0) {
        ++end;
    }
    return end - start;
}

bool ext2fs::ReadBlockGroups() {
    uint32_t blockGroups = superblock->total_inodes / superblock->inodes_per_group;
    if ((superblock->total_inodes % superblock->inodes_per_group) != 0) {
        ++blockGroups;
    }
    std::cout << "Total inodes of " << superblock->total_inodes << " and per group " << superblock->inodes_per_group
              << " gives block groups " << blockGroups << "\n";
    auto superblockBlock = 1024 / BlockSize;
    uint64_t blockGroupsBlock = superblockBlock + 1;
    uint64_t onDiskBlockGRoups = FsBlockToPhysBlock(blockGroupsBlock);
    uint64_t onDiskBlockGroupsOffset = FsBlockOffsetOnPhys(blockGroupsBlock);
    std::cout << "Superblock in block " << superblockBlock << " gives block groups in " << blockGroupsBlock << " on disk " << onDiskBlockGRoups << "\n";
    uint32_t blockGroupsSize = onDiskBlockGroupsOffset + (blockGroups * sizeof(ext2blockgroup));
    uint32_t blockGroupsTotalBlocks = blockGroupsSize / bdev->GetBlocksize();
    if ((blockGroupsSize % bdev->GetBlocksize()) != 0) {
        ++blockGroupsTotalBlocks;
    }
    std::cout << "Block groups size " << blockGroupsSize << " phys blocks " << blockGroupsTotalBlocks << "\n";
    auto physBlockGroups = bdev->ReadBlock(onDiskBlockGRoups, blockGroupsTotalBlocks);
    if (physBlockGroups) {
        groups.reserve(blockGroups);
        std::shared_ptr<ext2blockgroups> groups{new ext2blockgroups(blockGroups)};
        memcpy(&((*groups)[0]), ((uint8_t *) physBlockGroups->Pointer()) + onDiskBlockGroupsOffset, sizeof((*groups)[0]) * blockGroups);
        for (std::size_t i = 0; i < blockGroups; i++) {
            auto &groupObject = this->groups.emplace_back();
            auto &group = (*groups)[i];
            std::cout << "Block group " << group.free_blocks_count << " free block, " << group.free_inodes_count
            << " free inodes, " << group.block_bitmap << "/" << group.inode_bitmap << " block/inode bitmaps, "
            << group.inode_table << " inode table, " << group.used_dirs_count << " dirs.\n";
            auto blockBitmapBlock = FsBlockToPhysBlock(group.block_bitmap);
            auto blockBitmapOffset = FsBlockOffsetOnPhys(group.block_bitmap);
            auto blockBitmapPhysLength = FsBlocksToPhysBlocks(group.block_bitmap, 1);
            auto blockBitmapBlocks = bdev->ReadBlock(blockBitmapBlock, blockBitmapPhysLength);
            if (blockBitmapBlocks) {
                std::shared_ptr<ext2bitmap> blockBitmap{new ext2bitmap(superblock->blocks_per_group, BlockSize)};
                {
                    auto size = superblock->blocks_per_group >> 3;
                    if ((superblock->blocks_per_group & 7) != 0) {
                        ++size;
                    }
                    memcpy(blockBitmap->Pointer(), ((uint8_t *) blockBitmapBlocks->Pointer()) + blockBitmapOffset, size);
                }
                int free_count = 0;
                for (int i = 0; i < superblock->blocks_per_group; i++) {
                    if (!(*blockBitmap)[i]) {
                        ++free_count;
                    }
                }
                std::cout << "Found " << free_count << " free blocks at " << blockBitmapBlock << " offset "
                << blockBitmapOffset << " and num " << blockBitmapPhysLength << "\n";
                this->blockBitmap.push_back(blockBitmap);
            } else {
                std::cerr << "Error reading block group block bitmap\n";
                return false;
            }
            auto inodeBitmapBlock = FsBlockToPhysBlock(group.inode_bitmap);
            groupObject.InodeBitmapBlock = inodeBitmapBlock;
            auto inodeBitmapOffset = FsBlockOffsetOnPhys(group.inode_bitmap);
            auto inodeBitmapPhysLength = FsBlocksToPhysBlocks(group.inode_bitmap, 1);
            auto inodeBitmapBlocks = bdev->ReadBlock(inodeBitmapBlock, inodeBitmapPhysLength);
            if (inodeBitmapBlocks) {
                std::shared_ptr<ext2bitmap> inodeBitmap{new ext2bitmap(superblock->inodes_per_group, BlockSize)};
                {
                    auto size = superblock->inodes_per_group >> 3;
                    if ((superblock->inodes_per_group & 7) != 0) {
                        ++size;
                    }
                    memcpy(inodeBitmap->Pointer(), ((uint8_t *) inodeBitmapBlocks->Pointer()) + inodeBitmapOffset, size);
                }
                int free_count = 0;
                for (int i = 0; i < superblock->inodes_per_group; i++) {
                    if (!(*inodeBitmap)[i]) {
                        ++free_count;
                    }
                }
                std::cout << "Found " << free_count << " free inodes at " << inodeBitmapBlock << " offset "
                          << inodeBitmapOffset << " and num " << inodeBitmapPhysLength << "\n";
                this->inodeBitmap.push_back(inodeBitmap);
            } else {
                std::cerr << "Error reading block group inode bitmap\n";
                return false;
            }
            groupObject.InodeTableBlock = FsBlockToPhysBlock(group.inode_table);
            groupObject.InodeTableOffset = FsBlockOffsetOnPhys(group.inode_table);
            auto inodeTableSize = superblock->inodes_per_group * InodeSize();
            auto inodeTablePhysBlocks = (inodeTableSize + groupObject.InodeTableOffset) / bdev->GetBlocksize();
            auto inodeTableFileBlocks = (inodeTableSize + groupObject.InodeTableOffset) / FILEPAGE_PAGE_SIZE;
            if (((inodeTableSize + groupObject.InodeTableOffset) % FILEPAGE_PAGE_SIZE) != 0) {
                ++inodeTableFileBlocks;
            }
            std::cout << "Inode table starts at " << groupObject.InodeTableBlock << " offset " << groupObject.InodeTableOffset
            << " size of " << inodeTableSize << " and blocks " << inodeTablePhysBlocks << " internally represented as "
            << inodeTableFileBlocks << " pages\n";
            groupObject.InodeTableBlocks.reserve(inodeTableFileBlocks);
            for (std::size_t i = 0; i < inodeTableFileBlocks; i++) {
                groupObject.InodeTableBlocks.emplace_back();
            }
        }
        return true;
    } else {
        std::cerr << "Error reading block groups for ext2fs\n";
        return false;
    }
}

ext2fs_get_inode_result ext2fs::LoadInode(std::size_t inode_num) {
    std::unique_ptr<std::lock_guard<std::mutex>> lock{new std::lock_guard(mtx)};
    --inode_num;
    auto group_idx = inode_num / superblock->inodes_per_group;
    auto inode_off = inode_num % superblock->inodes_per_group;
    if (group_idx >= groups.size()) {
        return {.inode = {}, .status = filesystem_status::INVALID_REQUEST};
    }
    auto &group = groups[group_idx];
    auto offset = inode_off * InodeSize();
    offset += group.InodeTableOffset;
    auto end = offset + InodeSize() - 1;
    auto block_num = offset / FILEPAGE_PAGE_SIZE;
    offset = offset % FILEPAGE_PAGE_SIZE;
    auto block_end = end / FILEPAGE_PAGE_SIZE;
#ifdef DEBUG_INODE
    std::cout << "Inode " << (inode_num + 1) << " at " << block_num << " - " << block_end << " off "
    << offset << "\n";
#endif
    auto blocksize = bdev->GetBlocksize();
    auto rdStart = block_num * FILEPAGE_PAGE_SIZE;
    auto rdEnd = rdStart + FILEPAGE_PAGE_SIZE + blocksize - 1;
    auto rdOffset = rdStart % blocksize;
    rdStart = rdStart / blocksize;
    rdStart += group.InodeTableBlock;
    if (!group.InodeTableBlocks[block_num]) {
        lock = {};
        rdEnd = rdEnd / blocksize;
        rdEnd += group.InodeTableBlock;
        auto rd = bdev->ReadBlock(rdStart, rdEnd - rdStart);
        lock = std::make_unique<std::lock_guard<std::mutex>>(mtx);
        if (rd && !group.InodeTableBlocks[block_num]) {
            group.InodeTableBlocks[block_num] = std::make_shared<filepage>();
            memcpy(group.InodeTableBlocks[block_num]->Pointer()->Pointer(), ((uint8_t *) rd->Pointer()) + rdOffset, FILEPAGE_PAGE_SIZE);
        }
    }
    if ((block_num + 1) == block_end && !group.InodeTableBlocks[block_end]) {
        lock = {};
        auto rdStart = block_end * FILEPAGE_PAGE_SIZE; // intentionally scoped
        auto rdEnd = rdStart + FILEPAGE_PAGE_SIZE + blocksize - 1;
        auto rdOffset = rdStart % blocksize;
        rdStart = rdStart / blocksize;
        rdEnd = rdEnd / blocksize;
        auto rd = bdev->ReadBlock(rdStart + group.InodeTableBlock, rdEnd - rdStart);
        lock = std::make_unique<std::lock_guard<std::mutex>>(mtx);
        if (rd && !group.InodeTableBlocks[block_end]) {
            group.InodeTableBlocks[block_end] = std::make_shared<filepage>();
            memcpy(group.InodeTableBlocks[block_end]->Pointer()->Pointer(), ((uint8_t *) rd->Pointer()) + rdOffset, FILEPAGE_PAGE_SIZE);
        }
    }
    std::shared_ptr<ext2fs_inode> inode_obj{};
    if (block_num == block_end) {
        inode_obj = std::make_shared<ext2fs_inode>(self_ref, bdev, group.InodeTableBlocks[block_num], offset, BlockSize, rdStart);
    } else if ((block_num + 1) == block_end) {
        inode_obj = std::make_shared<ext2fs_inode>(self_ref, bdev, group.InodeTableBlocks[block_num], group.InodeTableBlocks[block_end], offset, BlockSize, rdStart);
    } else {
        return {};
    }
    ext2inode inode{};
    if (!inode_obj->Read(inode)) {
        return {.inode = {}, .status = filesystem_status::IO_ERROR};
    }
#ifdef DEBUG_INODE
    std::cout << "Inode " << inode_num << " " << std::oct << inode.mode << std::dec
              << " sz " << inode.size << " blks " << inode.blocks << ":";
#endif
    for (int i = 0; i < EXT2_NUM_DIRECT_BLOCK_PTRS; i++) {
        if (inode.block[i]) {
#ifdef DEBUG_INODE
            std::cout << " " << inode.block[i];
#endif
            inode_obj->blockRefs.push_back(inode.block[i]);
        }
    }
    lock = {};
    if (inode.size <= 60) {
        inode_obj->symlinkPointer.append((const char *) &(inode.block[0]), inode.size);
    } else {
        uint64_t indirect_block = inode.block[EXT2_NUM_DIRECT_BLOCK_PTRS];
        if (indirect_block != 0) {
#ifdef DEBUG_INODE
            std::cout << " <" << indirect_block << ">";
#endif
            indirect_block *= BlockSize;
            auto indirect_block_phys = indirect_block / bdev->GetBlocksize();
            auto indirect_block_offset = indirect_block % bdev->GetBlocksize();
            indirect_block += BlockSize - 1;
            auto indirect_phys_blocks = (indirect_block / bdev->GetBlocksize()) - indirect_block_phys + 1;
            auto rd = bdev->ReadBlock(indirect_block_phys, indirect_phys_blocks);
            if (rd) {
                uint32_t *indirect_bptrs = (uint32_t *) (((uint8_t *) (rd->Pointer())) + indirect_block_offset);
                auto numIndirects = BlockSize / sizeof(*indirect_bptrs);
                for (int i = 0; i < numIndirects; i++) {
                    if (indirect_bptrs[i]) {
#ifdef DEBUG_INODE
                        std::cout << " " << indirect_bptrs[i];
#endif
                        inode_obj->blockRefs.push_back(indirect_bptrs[i]);
                    }
                }
            } else {
                std::cerr << "Error: Read error, indirect block pointers for file\n";
                return {.inode = {}, .status = filesystem_status::IO_ERROR};
            }
        }
        uint64_t double_indirect_block = inode.block[EXT2_NUM_DIRECT_BLOCK_PTRS + 1];
        if (double_indirect_block != 0) {
#ifdef DEBUG_INODE
            std::cout << " <<" << double_indirect_block << ">>";
#endif
            double_indirect_block *= BlockSize;
            auto double_indirect_block_phys = double_indirect_block / bdev->GetBlocksize();
            auto double_indirect_block_offset = double_indirect_block % bdev->GetBlocksize();
            double_indirect_block += BlockSize - 1;
            auto double_indirect_phys_blocks = (double_indirect_block / bdev->GetBlocksize()) - double_indirect_block_phys + 1;
            auto dbrd = bdev->ReadBlock(double_indirect_block_phys, double_indirect_phys_blocks);
            if (dbrd) {
                uint32_t *double_indirect_bptrs = (uint32_t *) (((uint8_t *) (dbrd->Pointer())) + double_indirect_block_offset);
                auto numDblIndirects = BlockSize / sizeof(*double_indirect_bptrs);
                for (int i = 0; i < numDblIndirects; i++) {
                    uint64_t indirect_block = double_indirect_bptrs[i];
                    if (indirect_block != 0) {
#ifdef DEBUG_INODE
                        std::cout << " <" << indirect_block << ">";
#endif
                        indirect_block *= BlockSize;
                        auto indirect_block_phys = indirect_block / bdev->GetBlocksize();
                        auto indirect_block_offset = indirect_block % bdev->GetBlocksize();
                        indirect_block += BlockSize - 1;
                        auto indirect_phys_blocks = (indirect_block / bdev->GetBlocksize()) - indirect_block_phys + 1;
                        auto rd = bdev->ReadBlock(indirect_block_phys, indirect_phys_blocks);
                        if (rd) {
                            uint32_t *indirect_bptrs = (uint32_t *) (((uint8_t *) (rd->Pointer())) + indirect_block_offset);
                            auto numIndirects = BlockSize / sizeof(*indirect_bptrs);
                            for (int i = 0; i < numIndirects; i++) {
                                if (indirect_bptrs[i]) {
#ifdef DEBUG_INODE
                                    std::cout << " " << indirect_bptrs[i];
#endif
                                    inode_obj->blockRefs.push_back(indirect_bptrs[i]);
                                }
                            }
                        } else {
                            std::cerr << "Error: Read error, indirect block pointers for file\n";
                            return {.inode = {}, .status = filesystem_status::IO_ERROR};
                        }
                    }
                }
            } else {
                std::cerr << "Error: Read error, indirect block pointers for file\n";
                return {.inode = {}, .status = filesystem_status::IO_ERROR};
            }
        }
    }
    inode_obj->sys_dev_id = sys_dev_id;
    inode_obj->inode = inode_num + 1;
    inode_obj->filesize = inode.size;
    inode_obj->mode = inode.mode;
    auto pages = inode_obj->filesize / FILEPAGE_PAGE_SIZE;
    if ((inode_obj->filesize % FILEPAGE_PAGE_SIZE) != 0) {
        ++pages;
    }
    inode_obj->blockCache.reserve(pages);
    for (int i = 0; i < pages; i++) {
        inode_obj->blockCache.push_back({});
    }
#ifdef DEBUG_INODE
    std::cout << "\n";
#endif
    return {.inode = inode_obj, .status = filesystem_status::SUCCESS};
}

ext2fs_get_inode_result ext2fs::GetInode(std::size_t inode_num) {
    {
        std::lock_guard lock{mtx};
        for (auto &inode: inodes) {
            if (inode.inode_num == inode_num) {
                return {.inode = inode.inode, .status = filesystem_status::SUCCESS};
            }
        }
    }
    auto loadedInode = LoadInode(inode_num);
    if (loadedInode.inode) {
        std::lock_guard lock{mtx};
        for (auto &inode : inodes) {
            if (inode.inode_num == inode_num) {
                return {.inode = inode.inode, .status = filesystem_status::SUCCESS};
            }
        }
        ext2fs_inode_with_id with_id{.inode_num = (uint32_t) inode_num, .inode = loadedInode.inode};
        inodes.push_back(with_id);
        return {.inode = with_id.inode, .status = filesystem_status::SUCCESS};
    }
    return loadedInode;
}

class ext2fs_file : public fileitem {
protected:
    std::shared_ptr<filesystem> fs;
    std::shared_ptr<ext2fs_inode> inode;
public:
    ext2fs_file(std::shared_ptr<filesystem> fs, std::shared_ptr<ext2fs_inode> inode);
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
};

ext2fs_file::ext2fs_file(std::shared_ptr<filesystem> fs, std::shared_ptr<ext2fs_inode> inode) : fs(fs), inode(inode) {
}

uint32_t ext2fs_file::Mode() {
    return inode->mode;
}

std::size_t ext2fs_file::Size() {
    return inode->filesize;
}

uintptr_t ext2fs_file::InodeNum() {
    return inode->inode;
}

uint32_t ext2fs_file::BlockSize() {
    return inode->blocksize;
}

uintptr_t ext2fs_file::SysDevId() {
    return inode->sys_dev_id;
}

constexpr fileitem_status Convert(filesystem_status ino_stat) {
    fileitem_status status{fileitem_status::SUCCESS};
    switch (ino_stat) {
        case filesystem_status::SUCCESS:
            status = fileitem_status::SUCCESS;
            break;
        case filesystem_status::IO_ERROR:
            status = fileitem_status::IO_ERROR;
            break;
        case filesystem_status::INTEGRITY_ERROR:
            status = fileitem_status::INTEGRITY_ERROR;
            break;
        case filesystem_status::NOT_SUPPORTED_FS_FEATURE:
            status = fileitem_status::NOT_SUPPORTED_FS_FEATURE;
            break;
        case filesystem_status::INVALID_REQUEST:
            status = fileitem_status::INVALID_REQUEST;
            break;
        default:
            status = fileitem_status::IO_ERROR;
    }
    return status;
}

file_getpage_result ext2fs_file::GetPage(std::size_t pagenum) {
    auto result = inode->ReadBlockRaw(pagenum);
    return {.page = result.page, .status = Convert(result.status)};
}

file_read_result ext2fs_file::Read(uint64_t offset, void *ptr, std::size_t length) {
    auto result = inode->ReadBytes(offset, ptr, length);
    return {.size = result.size, .status = Convert(result.status)};
}

class ext2fs_symlink : public symlink, public ext2fs_file {
private:
    std::string symlink;
    bool loaded;
public:
    ext2fs_symlink(std::shared_ptr<filesystem> fs, std::shared_ptr<ext2fs_inode> inode);
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
    [[nodiscard]] std::string GetLink();
};

ext2fs_symlink::ext2fs_symlink(std::shared_ptr<filesystem> fs, std::shared_ptr<ext2fs_inode> inode) : ext2fs_file(fs, inode), symlink(), loaded(false) {
    if (inode->filesize <= 60) {
        this->symlink = inode->symlinkPointer;
        this->loaded = true;
    }
}

uint32_t ext2fs_symlink::Mode() {
    return ext2fs_file::Mode();
}

std::size_t ext2fs_symlink::Size() {
    return ext2fs_file::Size();
}

uintptr_t ext2fs_symlink::InodeNum() {
    return ext2fs_file::InodeNum();
}

uint32_t ext2fs_symlink::BlockSize() {
    return ext2fs_file::BlockSize();
}

uintptr_t ext2fs_symlink::SysDevId() {
    return ext2fs_file::SysDevId();
}

file_getpage_result ext2fs_symlink::GetPage(std::size_t pagenum) {
    return {.page = {}, .status = fileitem_status::NOT_SUPPORTED_FS_FEATURE};
}

file_read_result ext2fs_symlink::Read(uint64_t offset, void *ptr, std::size_t length) {
    return {.size = 0, .status = fileitem_status::NOT_SUPPORTED_FS_FEATURE};
}

std::string ext2fs_symlink::GetLink() {
    if (!loaded) {
        auto size = inode->filesize;
        char *buf = (char *) malloc(size);
        auto result = inode->ReadBytes(0, buf, size);
        if (result.status != filesystem_status::SUCCESS || result.size <= 0 || result.size > size) {
            free(buf);
            return {};
        }
        symlink.append((const char *) buf, result.size);
        free(buf);
        loaded = true;
    }
    return symlink;
}

class ext2fs_directory : public directory, public ext2fs_file {
private:
    std::vector<std::shared_ptr<directory_entry>> entries;
    std::size_t actualSize;
    bool entriesRead;
public:
    ext2fs_directory(std::shared_ptr<filesystem> fs, std::shared_ptr<ext2fs_inode> inode) : directory(), ext2fs_file(fs, inode), entries(), entriesRead(false) {}
    entries_result Entries() override;

    directory_resolve_result Create(std::string filename) override;

private:
    ext2fs &Filesystem() {
        filesystem *fs = &(*(this->fs));
        return *((ext2fs *) fs);
    }
    uint32_t Mode() override;
    std::size_t Size() override;
    uintptr_t InodeNum() override;
    uint32_t BlockSize() override;
    uintptr_t SysDevId() override;
    file_getpage_result GetPage(std::size_t pagenum) override;
    file_read_result Read(uint64_t offset, void *ptr, std::size_t length) override;
};

entries_result ext2fs_directory::Entries() {
    if (entriesRead) {
        return {.entries = entries, .status = fileitem_status::SUCCESS};
    }
    std::vector<std::shared_ptr<directory_entry>> entries{};
    ext2fs_inode_reader reader{inode};
    {
        ext2dirent baseDirent{};
        auto sizeRemaining = inode->GetFileSize();
        do {
            inode_read_bytes_result readResult{};
            if (sizeRemaining >= sizeof(baseDirent)) {
                readResult = reader.read(&baseDirent, sizeof(baseDirent));
            } else {
                readResult = {.size = 0, .status = filesystem_status::SUCCESS};
            }
            sizeRemaining -= readResult.size;
            if (readResult.size == sizeof(baseDirent) &&
                baseDirent.rec_len > sizeof(baseDirent) &&
                baseDirent.name_len > 0) {
                auto addLength = baseDirent.rec_len - sizeof(baseDirent);
                auto *dirent = new (malloc(baseDirent.rec_len)) ext2dirent();
                memcpy(dirent, &baseDirent, sizeof(*dirent));
                static_assert(sizeof(*dirent) == sizeof(baseDirent));
                if (sizeRemaining >= addLength) {
                    readResult = reader.read(((uint8_t *) dirent) + sizeof(baseDirent), addLength);
                    sizeRemaining -= readResult.size;
                } else {
                    readResult = {.size = 0, .status = filesystem_status::SUCCESS};
                }
                if (readResult.size == addLength) {
                    if (dirent->inode != 0) {
                        std::string name{dirent->Name()};
#ifdef DEBUG_DIR
                        std::cout << "Dir node " << dirent->inode << " type " << ((int) dirent->file_type) << " name "
                                  << name << "\n";
#endif
                        if (dirent->file_type == Ext2FileType_Directory) {
                            auto result = Filesystem().GetDirectory(fs, dirent->inode);
                            if (result.node) {
                                entries.emplace_back(new directory_entry(name, result.node));
                            } else {
                                return {.entries = {}, .status = Convert(result.status)};
                            }
                        } else if (dirent->file_type == Ext2FileType_Regular) {
                            auto result = Filesystem().GetFile(fs, dirent->inode);
                            if (result.node) {
                                entries.emplace_back(new directory_entry(name, result.node));
                            } else {
                                return {.entries = {}, .status = Convert(result.status)};
                            }
                        } else if (dirent->file_type == Ext2FileType_Symlink) {
                            auto result = Filesystem().GetSymlink(fs, dirent->inode);
                            if (result.node) {
                                entries.emplace_back(new directory_entry(name, result.node));
                            } else {
                                return {.entries = {}, .status = Convert(result.status)};
                            }
                        }
                    }
                } else {
                    if (readResult.status != filesystem_status::SUCCESS) {
                        return {.entries = {}, .status = Convert(readResult.status)};
                    }
                    baseDirent.inode = 0;
                }
                dirent->~ext2dirent();
                free(dirent);
            } else {
                if (readResult.status != filesystem_status::SUCCESS) {
                    return {.entries = {}, .status = Convert(readResult.status)};
                }
                baseDirent.inode = 0;
            }
        } while (baseDirent.inode != 0);
        actualSize = inode->GetFileSize() - sizeRemaining;
    }
    if (!entriesRead) {
        this->entries = entries;
        entriesRead = true;
    }
    return {.entries = this->entries, .status = fileitem_status::SUCCESS};
}

uint32_t ext2fs_directory::Mode() {
    return ext2fs_file::Mode();
}

std::size_t ext2fs_directory::Size() {
    return ext2fs_file::Size();
}

uintptr_t ext2fs_directory::InodeNum() {
    return ext2fs_file::InodeNum();
}

uint32_t ext2fs_directory::BlockSize() {
    return ext2fs_file::BlockSize();
}

uintptr_t ext2fs_directory::SysDevId() {
    return ext2fs_file::SysDevId();
}

file_getpage_result ext2fs_directory::GetPage(std::size_t pagenum) {
    return {.page = {}, .status = fileitem_status::INVALID_REQUEST};
}

file_read_result ext2fs_directory::Read(uint64_t offset, void *ptr, std::size_t length) {
    return {.size = 0, .status = fileitem_status::INVALID_REQUEST};
}

directory_resolve_result ext2fs_directory::Create(std::string filename) {
    auto allocInode = Filesystem().AllocateInode();
    if (allocInode.status != filesystem_status::SUCCESS) {
        fileitem_status status;
        switch (allocInode.status) {
            case filesystem_status::IO_ERROR:
                status = fileitem_status::IO_ERROR;
                break;
            case filesystem_status::INTEGRITY_ERROR:
                status = fileitem_status::INTEGRITY_ERROR;
                break;
            case filesystem_status::NOT_SUPPORTED_FS_FEATURE:
                status = fileitem_status::NOT_SUPPORTED_FS_FEATURE;
                break;
            case filesystem_status::INVALID_REQUEST:
                status = fileitem_status::INVALID_REQUEST;
                break;
            case filesystem_status::NO_AVAIL_INODES:
                status = fileitem_status::NO_AVAIL_INODES;
                break;
            default:
                status = fileitem_status::IO_ERROR;
        }
        return {.file = {}, .status = status};
    }
    auto readResult = Entries();
    if (readResult.status != fileitem_status::SUCCESS) {
        return {.file = {}, .status = readResult.status};
    }
    allocInode.inode->Init();
    allocInode.inode->dirty = true;
    std::shared_ptr<ext2fs_file> file = std::make_shared<ext2fs_file>(fs, allocInode.inode);

    auto seekTo = actualSize;
    ext2fs_inode_reader reader{inode};
    reader.seek_set(seekTo);

    ext2dirent *dirent;
    dirent = new (malloc(sizeof(*dirent) + filename.size())) ext2dirent();
    dirent->inode = allocInode.inode->inode;
    dirent->file_type = Ext2FileType_Regular;
    dirent->name_len = filename.size();
    dirent->rec_len = sizeof(*dirent) + filename.size();
    char *filenameArea = ((char *) dirent) + sizeof(*dirent);
    memcpy(filenameArea, filename.data(), filename.size());

    actualSize += dirent->rec_len;
    if (actualSize > inode->GetFileSize()) {
        auto blocksize = BlockSize();
        auto blocks = actualSize / blocksize;
        if ((actualSize % blocksize) != 0) {
            ++blocks;
        }
        inode->SetFileSize(blocks * blocksize);
    }

    auto result = reader.write(dirent, dirent->rec_len);
    dirent->~ext2dirent();
    free(dirent);
    if (result.status != filesystem_status::SUCCESS) {
        switch (result.status) {
            case filesystem_status::IO_ERROR:
                return {.file = {}, .status = fileitem_status::IO_ERROR};
            case filesystem_status::INTEGRITY_ERROR:
                return {.file = {}, .status = fileitem_status::INTEGRITY_ERROR};
            case filesystem_status::NOT_SUPPORTED_FS_FEATURE:
                return {.file = {}, .status = fileitem_status::NOT_SUPPORTED_FS_FEATURE};
            case filesystem_status::INVALID_REQUEST:
                return {.file = {}, .status = fileitem_status::INVALID_REQUEST};
            case filesystem_status::NO_AVAIL_INODES:
                return {.file = {}, .status = fileitem_status::NO_AVAIL_INODES};
            case filesystem_status::NO_AVAIL_BLOCKS:
                return {.file = {}, .status = fileitem_status::NO_AVAIL_BLOCKS};
        }
    }

    return {.file = file, .status = fileitem_status::SUCCESS};
}

filesystem_get_node_result<directory> ext2fs::GetDirectory(std::shared_ptr<filesystem> shared_this, std::size_t inode_num) {
    if (!shared_this || this != ((ext2fs *) &(*shared_this))) {
        std::cerr << "Wrong shared reference for filesystem when opening directory\n";
        return {.node = {}, .status = filesystem_status::INVALID_REQUEST};
    }
    auto inode_obj = GetInode(inode_num);
    if (!inode_obj.inode) {
        std::cerr << "Failed to open inode " << inode_num << "\n";
        return {.node = {}, .status = inode_obj.status};
    }
    return {.node = std::make_shared<ext2fs_directory>(shared_this, inode_obj.inode), .status = filesystem_status::SUCCESS};
}

filesystem_get_node_result<fileitem> ext2fs::GetFile(std::shared_ptr<filesystem> shared_this, std::size_t inode_num) {
    if (!shared_this || this != ((ext2fs *) &(*shared_this))) {
        std::cerr << "Wrong shared reference for filesystem when opening directory\n";
        return {.node = {}, .status = filesystem_status::INVALID_REQUEST};
    }
    auto inode_obj = GetInode(inode_num);
    if (!inode_obj.inode) {
        std::cerr << "Failed to open inode " << inode_num << "\n";
        return {.node = {}, .status = inode_obj.status};
    }
    return {.node = std::make_shared<ext2fs_file>(shared_this, inode_obj.inode), .status = filesystem_status::SUCCESS};
}

filesystem_get_node_result<fileitem> ext2fs::GetSymlink(std::shared_ptr<filesystem> shared_this, std::size_t inode_num) {
    if (!shared_this || this != ((ext2fs *) &(*shared_this))) {
        std::cerr << "Wrong shared reference for filesystem when opening directory\n";
        return {.node = {}, .status = filesystem_status::INVALID_REQUEST};
    }
    auto inode_obj = GetInode(inode_num);
    if (!inode_obj.inode) {
        std::cerr << "Failed to open inode " << inode_num << "\n";
        return {.node = {}, .status = inode_obj.status};
    }
    std::shared_ptr<ext2fs_symlink> symlink = std::make_shared<ext2fs_symlink>(shared_this, inode_obj.inode);
    std::shared_ptr<class symlink> as_symlink{symlink};
    return {.node = as_symlink, .status = filesystem_status::SUCCESS};
}

ext2fs_get_inode_result ext2fs::AllocateInode() {
    std::unique_ptr<std::lock_guard<std::mutex>> lock{new std::lock_guard(mtx)};
    for (auto i = 0; i < inodeBitmap.size(); i++) {
        auto inodeNum = inodeBitmap[i]->FindFree();
        if (inodeNum == 0) {
            continue;
        }
        (*(inodeBitmap[i]))[inodeNum - 1] = true;
        lock = {};
        inodeNum += i * superblock->inodes_per_group;
        return GetInode(inodeNum);
    }
    return {.inode = {}, .status = filesystem_status::NO_AVAIL_INODES};
}

ext2fs_allocate_blocks_result ext2fs::AllocateBlocks(std::size_t requestedCount) {
    if (requestedCount <= 0) {
        return {.block = 0, .count = 0, .status = filesystem_status::SUCCESS};
    }
    std::lock_guard lock{mtx};
    for (auto i = 0; i < blockBitmap.size(); i++) {
        auto block = blockBitmap[i]->FindFree();
        if (block == 0) {
            continue;
        }
        --block;
        (*(blockBitmap[i]))[block] = true;
        ext2fs_allocate_blocks_result result{.block = (superblock->blocks_per_group * i) + block, .count = 1, .status = filesystem_status::SUCCESS};
        --requestedCount;
        while (requestedCount > 0) {
            ++block;
            if (block >= superblock->blocks_per_group || (*(blockBitmap[i]))[block]) {
                break;
            }
            (*(blockBitmap[i]))[block] = true;
            --requestedCount;
            ++result.count;
        }
        return result;
    }
    return {.block = 0, .count = 0, .status = filesystem_status::NO_AVAIL_BLOCKS};
}

filesystem_status ext2fs::ReleaseBlock(uint32_t blknum) {
    auto group = blknum / superblock->blocks_per_group;
    auto blk = blknum % superblock->blocks_per_group;
    std::lock_guard lock{mtx};
    (*(blockBitmap[group]))[blk] = false;
}

std::vector<std::vector<dirty_block>> ext2fs::GetWrites() {
    std::vector<std::vector<dirty_block>> blocks{};
    {
        std::lock_guard lock{mtx};
        for (auto &inode: inodes) {
            auto writeGroups = inode.inode->GetWrites();
            auto destIterator = blocks.begin();
            auto sourceIterator = writeGroups.begin();
            while (sourceIterator != writeGroups.end() && destIterator != blocks.end()) {
                for (const auto &wr : *sourceIterator) {
                    destIterator->emplace_back(wr);
                }
                ++sourceIterator;
                ++destIterator;
            }
            while (sourceIterator != writeGroups.end()) {
                auto &dest = blocks.emplace_back();
                for (const auto &wr : *sourceIterator) {
                    dest.emplace_back(wr);
                }
                ++sourceIterator;
            }
        }
    }
    {
        std::vector<dirty_block> bitmapBlocks{};
        std::remove_const<typeof(groups.size())>::type groupIdx = 0;
        while (groupIdx < groups.size()) {
            const auto &group = groups[groupIdx];
            const auto &inodeBitmap = this->inodeBitmap[groupIdx];
            const auto &blockBitmap = this->blockBitmap[groupIdx];
            auto inodeBlockNums = inodeBitmap->DirtyBlocks();
            if (!inodeBlockNums.empty()) {
                for (auto bmBlock : inodeBlockNums) {
                    const void *blockPtr = inodeBitmap->PointerToBlock(bmBlock);
                    std::shared_ptr<filepage> page = std::make_shared<filepage>();
                    memcpy(page->Pointer()->Pointer(), blockPtr, BlockSize);
                    uint64_t physBlock{bmBlock};
                    physBlock = physBlock * BlockSize;
                    physBlock = physBlock / bdev->GetBlocksize();
                    dirty_block dirtyBlock{.page1 = page, .page2 = {}, .blockaddr = group.InodeBitmapBlock + physBlock, .offset = 0, .length = BlockSize};
                    bitmapBlocks.emplace_back(dirtyBlock);
                }
            }
            ++groupIdx;
        }
        if (!bitmapBlocks.empty()) {
            if (blocks.empty()) {
                blocks.emplace_back();
            }
            auto &writeList = blocks[0];
            for (const auto &bitmapBlock : bitmapBlocks) {
                writeList.emplace_back(bitmapBlock);
            }
        }
    }
    return blocks;
}

filesystem_get_node_result<directory> ext2fs::GetRootDirectory(std::shared_ptr<filesystem> shared_this) {
    return GetDirectory(shared_this, 2);
}

ext2fs_provider::ext2fs_provider() {
}

std::string ext2fs_provider::name() const {
    return "ext2fs";
}

std::shared_ptr<blockdev_filesystem> ext2fs_provider::open(std::shared_ptr<blockdev> bdev) const {
    std::shared_ptr<ext2fs> fs{new ext2fs(bdev)};
    {
        std::weak_ptr<ext2fs> weakPtr{fs};
        fs->self_ref = weakPtr;
    }
    if (fs->HasSuperblock() && fs->FsSignature() == EXT2_SIGNATURE) {
        std::cout << "Ext2 signature " << fs->FsSignature() << " " << fs->VersionMajor() << "." << fs->VersionMinor() << "\n";
        if (!fs->ReadBlockGroups()) {
            return {};
        }
        return fs;
    } else {
        return {};
    }
}
