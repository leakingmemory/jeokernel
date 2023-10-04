//
// Created by sigsegv on 4/25/22.
//

#include "ext2fs.h"
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
    return blocks;
}

filesystem_get_node_result<directory> ext2fs::GetRootDirectory(std::shared_ptr<filesystem> shared_this) {
    return GetDirectory(shared_this, 2);
}

bool ext2fs_inode::Init() {
    ext2inode inode{};
    if (!Read(inode)) {
        return false;
    }
    bzero(&inode, sizeof(inode));
    inode.mode = 644;
    if (!Write(inode)) {
        return false;
    }
    return true;
}

bool ext2fs_inode::Read(ext2inode &inode) {
    constexpr auto blocksize = FILEPAGE_PAGE_SIZE;
    auto sz1 = sizeof(inode);
    if ((sz1 + offset) > blocksize) {
        sz1 = blocksize - offset;
        auto sz2 = sizeof(inode) - sz1;
        if (!blocks[1]) {
            return false;
        }
        memcpy(((uint8_t *) (void *) &inode) + sz1, blocks[1]->Pointer()->Pointer(), sz2);
    }
    if (!blocks[0]) {
        return false;
    }
    memcpy(&inode, ((uint8_t *) blocks[0]->Pointer()->Pointer()) + offset, sz1);
    return true;
}

bool ext2fs_inode::Write(ext2inode &inode) {
    constexpr auto blocksize = FILEPAGE_PAGE_SIZE;
    auto sz1 = sizeof(inode);
    if ((sz1 + offset) > blocksize) {
        sz1 = blocksize - offset;
        auto sz2 = sizeof(inode) - sz1;
        if (!blocks[1]) {
            return false;
        }
        memcpy(blocks[1]->Pointer()->Pointer(), ((uint8_t *) (void *) &inode) + sz1, sz2);
    }
    if (!blocks[0]) {
        return false;
    }
    memcpy(((uint8_t *) blocks[0]->Pointer()->Pointer()) + offset, &inode, sz1);
    return true;
}

size_t ext2fs_inode::WriteSize() const {
    return sizeof(struct ext2inode);
}

class ext2file_block : public blockdev_block {
    friend ext2fs_inode;
private:
    void *ptr;
    std::size_t size;
public:
    ext2file_block(std::size_t size) : ptr(nullptr), size(size) {
        ptr = malloc(size);
    }
    ext2file_block(const ext2file_block &) = delete;
    ext2file_block(ext2file_block &&) = delete;
    ext2file_block &operator =(const ext2file_block &) = delete;
    ext2file_block &operator =(ext2file_block &&) = delete;
    virtual ~ext2file_block();
    void *Pointer() const override;
    std::size_t Size() const override;
};

ext2file_block::~ext2file_block() {
    if (ptr != nullptr) {
        free(ptr);
        ptr = nullptr;
    }
}

void *ext2file_block::Pointer() const {
    return ptr;
}

std::size_t ext2file_block::Size() const {
    return size;
}

inode_read_blocks_result ext2fs_inode::ReadBlocks(uint32_t startingBlock, uint32_t startingOffset, uint32_t length) {
    std::vector<std::shared_ptr<blockdev_block>> readBlocks{};
    while (startingOffset >= blocksize) {
        ++startingBlock;
        startingOffset -= blocksize;
    }
    uint32_t numBlocks = startingOffset + length;
    uint32_t lastBlockLength = numBlocks % blocksize;
    if (lastBlockLength == 0) {
        lastBlockLength = blocksize;
    }
    numBlocks += blocksize - lastBlockLength;
    numBlocks /= blocksize;
    uint32_t firstReadLength = 0;
    for (auto blockNo = startingBlock; blockNo < (startingBlock + numBlocks); ++blockNo) {
        if (blockNo < blockRefs.size()) {
            auto fsBlock = blockRefs[blockNo];
            if (fsBlock != 0) {
                uint64_t startingAddr = fsBlock;
                startingAddr *= blocksize;
                if (blockNo == startingBlock) {
                    startingAddr += startingOffset;
                }
                uint64_t physBlock = startingAddr / bdev->GetBlocksize();
                uint64_t endingOffset = fsBlock + 1;
                if ((blockNo + 1) == (startingBlock + numBlocks)) {
                    endingOffset = startingAddr + lastBlockLength;
                } else {
                    endingOffset *= blocksize;
                }
                uint32_t offset = startingAddr % bdev->GetBlocksize();
                startingAddr = physBlock * bdev->GetBlocksize();
                uint32_t physBlocks = (endingOffset - startingAddr) / bdev->GetBlocksize();
                if (blockNo == startingBlock) {
                    firstReadLength = endingOffset - startingAddr - offset;
                }
                if (((endingOffset - startingAddr) % bdev->GetBlocksize()) != 0) {
                    ++physBlocks;
                }
#ifdef DEBUG_INODE
                std::cout << "Read file block " << blockNo << ": phys " << physBlock << " num " << physBlocks << "\n";
#endif
                auto rdBlock = bdev->ReadBlock(physBlock, physBlocks);
                if (rdBlock && offset != 0) {
                    memcpy(rdBlock->Pointer(), ((uint8_t *) rdBlock->Pointer()) + offset, blocksize);
                }
                if (!rdBlock) {
                    std::cerr << "Read error file block " << blockNo << "\n";
                    return {.block = {}, .status = filesystem_status::IO_ERROR};
                }
                readBlocks.push_back(rdBlock);
            } else {
                return {.block = {}, .status = filesystem_status::NOT_SUPPORTED_FS_FEATURE};
            }
        }
    }
    std::shared_ptr<blockdev_block> finalBlock{new ext2file_block(length)};
    bzero(finalBlock->Pointer(), length);
    int i = 0;
    uint32_t offset = 0;
    if (i < readBlocks.size()) {
        auto cp = firstReadLength;
        if (cp > length) {
            cp = length;
        }
        if (readBlocks[i]) {
#ifdef DEBUG_INODE
            std::cout << "Assemble blocks 0 - " << cp << "\n";
#endif
            memcpy(finalBlock->Pointer(), readBlocks[i]->Pointer(), cp);
        }
        offset = firstReadLength;
        ++i;
    }
    if (readBlocks.size() > 1) {
        while (i < (readBlocks.size() - 1)) {
            if (readBlocks[i]) {
#ifdef DEBUG_INODE
                std::cout << "Assemble blocks " << offset << " - " << (offset + blocksize) << "\n";
#endif
                memcpy(((uint8_t *) finalBlock->Pointer()) + offset, readBlocks[i]->Pointer(), blocksize);
            }
            offset += blocksize;
            ++i;
        }
        if (i < readBlocks.size()) {
            if (readBlocks[i]) {
#ifdef DEBUG_INODE
                std::cout << "Assemble blocks " << offset << " - " << (offset + lastBlockLength) << "\n";
#endif
                memcpy(((uint8_t *) finalBlock->Pointer()) + offset, readBlocks[i]->Pointer(), lastBlockLength);
            }
        }
    }
    return {.block = finalBlock, .status = filesystem_status::SUCCESS};
}

filesystem_status ext2fs_inode::AllocToExtend(std::size_t blki, uint32_t sizeAlloc) {
    std::shared_ptr<ext2fs> fs = this->fs.lock();
    if (!fs) {
        return filesystem_status::IO_ERROR;
    }
    std::unique_ptr<std::lock_guard<std::mutex>> lock{new std::lock_guard(mtx)};
    auto neededBlocks = blockRefs.size();
    lock = {};

    uint64_t startingAddr = blki;
    startingAddr *= FILEPAGE_PAGE_SIZE;
    uint64_t endingAddr = startingAddr + (sizeAlloc == 0 ? FILEPAGE_PAGE_SIZE : sizeAlloc);
    uint64_t startingBlock = startingAddr / blocksize;
    uint64_t endingBlock = endingAddr / blocksize;
    {
        uint64_t endingBlockAddr = endingBlock * blocksize;
        if (endingBlockAddr < endingAddr) {
            ++endingBlock;
        }
    }
    if (endingBlock > neededBlocks) {
        neededBlocks = endingBlock - neededBlocks;
    } else {
        neededBlocks = 0;
    }

    std::vector<uint32_t> blocks{};

    for (typeof(neededBlocks) i = 0; i < neededBlocks; i++) {
        auto allocBlockResult = fs->AllocateBlocks(1);
        if (allocBlockResult.status != filesystem_status::SUCCESS || allocBlockResult.count != 1) {
            for (auto blk : blocks) {
                fs->ReleaseBlock(blk);
            }
            return allocBlockResult.count == 1 ? allocBlockResult.status : filesystem_status::NO_AVAIL_BLOCKS;
        }
        blocks.emplace_back(allocBlockResult.block);
    }

    lock = std::make_unique<std::lock_guard<std::mutex>>(mtx);
    for (auto blk : blocks) {
        blockRefs.push_back(blk);
    }
    while (blki > blockCache.size()) {
        auto &ref = blockCache.emplace_back(new filepage());
        ref->Zero();
        ref->SetDirty(FILEPAGE_PAGE_SIZE);
    }
    if (blki >= blockCache.size()) {
        auto &ref = blockCache.emplace_back(new filepage());
        ref->Zero();
        ref->SetDirty(sizeAlloc);
    }
    return filesystem_status::SUCCESS;
}

inode_read_blocks_raw_result ext2fs_inode::ReadBlockRaw(std::size_t blki, bool alloc, uint32_t sizeAlloc) {
    if (alloc) {
        AllocToExtend(blki, sizeAlloc);
    }
    std::shared_ptr<ext2fs> fs = this->fs.lock();
    if (!fs) {
        return {.page = {}, .status = filesystem_status::IO_ERROR};
    }
    std::unique_ptr<std::lock_guard<std::mutex>> lock{new std::lock_guard(mtx)};
    if (blki < blockCache.size()) {
        if (!blockCache[blki]) {
            uint64_t startingAddr = blki;
            startingAddr *= FILEPAGE_PAGE_SIZE;
            uint64_t endingAddr = startingAddr + FILEPAGE_PAGE_SIZE;
            uint64_t startingBlock = startingAddr / blocksize;
            uint64_t endingBlock = endingAddr / blocksize;
            {
                uint64_t endingBlockAddr = endingBlock * blocksize;
                if (endingBlockAddr < endingAddr) {
                    ++endingBlock;
                }
            }
            auto readingOffset = startingAddr % blocksize;
            lock = {};
            auto readBlocks = ReadBlocks(startingBlock, readingOffset, FILEPAGE_PAGE_SIZE);
            if (readBlocks.block) {
                std::shared_ptr<filepage> page{new filepage()};
                memcpy(page->Pointer()->Pointer(), readBlocks.block->Pointer(), FILEPAGE_PAGE_SIZE);
                std::lock_guard lock{mtx};
                if (!blockCache[blki]) {
                    blockCache[blki] = page;
                }
                return {.page = blockCache[blki], .status = filesystem_status::SUCCESS};
            } else {
                std::cerr << "Read error for inode block " << startingBlock << "\n";
                return {.page = {}, .status = filesystem_status::IO_ERROR};
            }
        }
        return {.page = blockCache[blki], .status = filesystem_status::SUCCESS};
    }
    return {.page = {}, .status = filesystem_status::INVALID_REQUEST};
}

inode_read_block_result ext2fs_inode::ReadBlock(std::size_t blki) {
    auto block = ReadBlockRaw(blki);
    if (block.page) {
        return {.page = block.page->Pointer(), .status = block.status};
    }
    return {.page = {}, .status = block.status};
}

inode_read_block_result ext2fs_inode::ReadOrAllocBlock(std::size_t blki, std::size_t sizeAlloc) {
    auto block = ReadBlockRaw(blki, true, sizeAlloc);
    if (block.page) {
        return {.page = block.page->Pointer(), .status = block.status};
    }
    return {.page = {}, .status = block.status};
}

inode_read_bytes_result ext2fs_inode::ReadBytes(uint64_t offset, void *ptr, std::size_t length) {
    uint8_t *p = (uint8_t *) ptr;
    if ((offset + ((uint64_t) length)) > filesize) {
        if (offset >= filesize) {
            return {.size = 0, .status = filesystem_status::SUCCESS};
        }
        length = (std::size_t) (filesize - offset);
    }
    std::size_t vec = 0;
    while (vec < length) {
        uint64_t cur = offset + vec;
        uint64_t blk = cur / FILEPAGE_PAGE_SIZE;
        uint32_t blkoff = (uint32_t) (cur % FILEPAGE_PAGE_SIZE);
        uint32_t blklen = FILEPAGE_PAGE_SIZE;
        if (blklen > (length - vec)) {
            blklen = length - vec;
        }
        if (blklen > (FILEPAGE_PAGE_SIZE - blkoff)) {
            blklen = FILEPAGE_PAGE_SIZE - blkoff;
        }

        auto blkp = ReadBlock(blk);
        if (blkp.page) {
            memcpy(p + vec, ((uint8_t *) blkp.page->Pointer()) + blkoff, blklen);
        } else {
            return {.size = vec, .status = blkp.status};
        }

        vec += blklen;
    }
    return {.size = length, .status = filesystem_status::SUCCESS};
}

inode_read_bytes_result ext2fs_inode::WriteBytes(uint64_t offset, void *ptr, std::size_t length) {
    uint8_t *p = (uint8_t *) ptr;
    std::size_t vec = 0;
    while (vec < length) {
        uint64_t cur = offset + vec;
        uint64_t blk = cur / FILEPAGE_PAGE_SIZE;
        uint32_t blkoff = (uint32_t) (cur % FILEPAGE_PAGE_SIZE);
        uint32_t blklen = FILEPAGE_PAGE_SIZE;
        if (blklen > (length - vec)) {
            blklen = length - vec;
        }
        if (blklen > (FILEPAGE_PAGE_SIZE - blkoff)) {
            blklen = FILEPAGE_PAGE_SIZE - blkoff;
        }

        auto requestedLength = length - vec;

        auto blkp = ReadOrAllocBlock(blk, blkoff + blklen);
        if (blkp.page) {
            memcpy(((uint8_t *) blkp.page->Pointer()) + blkoff, p + vec, blklen);
            blkp.page->SetDirty(blkoff + blklen);
        } else {
            if ((offset + ((uint64_t) vec)) > filesize) {
                filesize = offset + ((uint64_t) vec);
                dirty = true;
            }
            return {.size = vec, .status = blkp.status};
        }

        vec += blklen;
    }
    return {.size = length, .status = filesystem_status::SUCCESS};
}

void ext2fs_inode::SetFileSize(uint64_t filesize) {
    this->filesize = filesize;
    this->dirty = true;
}

std::vector<std::vector<dirty_block>> ext2fs_inode::GetWrites() {
    std::vector<dirty_block> inodeBlocks{};
    auto dirty = this->dirty;
    if (dirty) {
        ext2inode inode{};
        if (!Read(inode)) {
            return {};
        }
        inode.size = filesize;
        {
            uint64_t i_blocks = blockRefs.size();
            i_blocks = i_blocks * blocksize;
            i_blocks = i_blocks / 512;
            inode.blocks = i_blocks;
        }
        auto blockIterator = blockRefs.begin();
        {
            int i = 0;
            while (i < EXT2_NUM_DIRECT_BLOCK_PTRS && blockIterator != blockRefs.end()) {
                inode.block[i] = *blockIterator;
                ++blockIterator;
                ++i;
            }
            while (i < EXT2_NUM_DIRECT_BLOCK_PTRS) {
                inode.block[i] = 0;
                ++i;
            }
        }
        if (!Write(inode)) {
            return {};
        }
        dirty_block blk{.page1 = blocks[0], .page2 = blocks[1], .blockaddr = blocknum, .offset = 0, .length = (uint32_t) (GetOffset() + WriteSize())};
        inodeBlocks.emplace_back(blk);
        this->dirty = false;
    }

    std::vector<std::vector<dirty_block>> writeGroups{};
    {
        std::vector<dirty_block> blocks{};
        for (int i = 0; i < blockCache.size(); i++) {
            auto block = blockCache[i];
            if (block && block->IsDirty()) {
                uint64_t startingAddr = i;
                startingAddr *= FILEPAGE_PAGE_SIZE;
                uint64_t endingAddr = startingAddr + block->GetDirtyLength();
                uint64_t startingBlock = startingAddr / blocksize;
                uint64_t endingBlock = endingAddr / blocksize;
                if ((endingAddr % blocksize) == 0) {
                    --endingBlock;
                }

                for (typeof(startingBlock) i = startingBlock; i <= endingBlock; i++) {
                    auto addr = i * blocksize;
                    uint32_t fileblock = (uint32_t) (addr / FILEPAGE_PAGE_SIZE);
                    uint32_t fileblock_offset = (uint32_t) (addr % FILEPAGE_PAGE_SIZE);
                    uint64_t blockaddr = blockRefs[i];
                    blockaddr = blockaddr * blocksize;
                    blockaddr /= bdev->GetBlocksize();
                    dirty_block blk{.page1 = blockCache[fileblock], .page2 = {}, .blockaddr = blockaddr, .offset = fileblock_offset, .length = (uint32_t) blocksize};
                    if ((fileblock_offset + blocksize) > FILEPAGE_PAGE_SIZE) {
                        blk.page2 = blockCache[fileblock + 1];
                    }
                    blocks.emplace_back(blk);
                }
            }
        }
        writeGroups.push_back(blocks);
    }
    if (!inodeBlocks.empty()) {
        writeGroups.emplace_back(inodeBlocks);
    }
    return writeGroups;
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

filesystem_status ext2fs_inode_reader::seek_set(std::size_t offset) {
    next_blki = offset / FILEPAGE_PAGE_SIZE;
    this->offset = offset % FILEPAGE_PAGE_SIZE;
    cur_blki = next_blki;
    auto result = inode->ReadBlock(cur_blki);
    page = result.page;
    next_blki++;
    return result.status;
}

inode_read_bytes_result ext2fs_inode_reader::read(void *ptr, std::size_t bytes) {
    if (page) {
        auto remaining = FILEPAGE_PAGE_SIZE - offset;
        if (bytes <= remaining) {
            memcpy(ptr, ((uint8_t *) page->Pointer()) + offset, bytes);
            offset += bytes;
            if (offset == FILEPAGE_PAGE_SIZE) {
                page = {};
                offset = 0;
            }
            return {.size = bytes, .status = filesystem_status::SUCCESS};
        } else {
            memcpy(ptr, ((uint8_t *) page->Pointer()) + offset, remaining);
            page = {};
            offset = 0;
            auto nextrd = read(((uint8_t *) ptr) + remaining, bytes - remaining);
            return {.size = remaining + nextrd.size, .status = nextrd.status};
        }
    } else {
        cur_blki = next_blki;
        auto result = inode->ReadBlock(cur_blki);
        page = result.page;
        ++next_blki;
        if (!page) {
            return {.size = 0, .status = result.status};
        }
        return read(ptr, bytes);
    }
}

inode_read_bytes_result ext2fs_inode_reader::write(const void *ptr, std::size_t bytes) {
    if (page) {
        auto remaining = FILEPAGE_PAGE_SIZE - offset;
        if (bytes <= remaining) {
            inode->AllocToExtend(cur_blki, offset + bytes);
            memcpy(((uint8_t *) page->Pointer()) + offset, ptr, bytes);
            page->SetDirty(offset + bytes);
            offset += bytes;
            if (offset == FILEPAGE_PAGE_SIZE) {
                page = {};
                offset = 0;
            }
            return {.size = bytes, .status = filesystem_status::SUCCESS};
        } else {
            inode->AllocToExtend(cur_blki, offset + remaining);
            memcpy(((uint8_t *) page->Pointer()) + offset, ptr, remaining);
            page->SetDirty(offset + remaining);
            page = {};
            offset = 0;
            auto nextrd = write(((uint8_t *) ptr) + remaining, bytes - remaining);
            return {.size = remaining + nextrd.size, .status = nextrd.status};
        }
    } else {
        auto length = offset + bytes;
        if (length > FILEPAGE_PAGE_SIZE) {
            length = FILEPAGE_PAGE_SIZE;
        }
        cur_blki = next_blki;
        auto result = inode->ReadOrAllocBlock(cur_blki, length);
        page = result.page;
        ++next_blki;
        if (!page) {
            return {.size = 0, .status = result.status};
        }
        return write(ptr, bytes);
    }
}