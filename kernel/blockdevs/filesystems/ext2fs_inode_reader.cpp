//
// Created by sigsegv on 10/5/23.
//

#include "ext2fs_inode_reader.h"
#include "ext2fs_inode.h"
#include <cstring>
#include <files/fsreference.h>

ext2fs_inode_reader::ext2fs_inode_reader() : fsreferrer("ext2fs_inode_reader"), inode(), page(), offset(0), next_blki(0), cur_blki(0) {
}

void ext2fs_inode_reader::Init(fsresource<ext2fs_inode> &inode) {
    std::shared_ptr<fsreferrer> ref = self_ref.lock();
    this->inode = inode.CreateReference(ref);
}

void ext2fs_inode_reader::Init(const fsreference<ext2fs_inode> &inode) {
    std::shared_ptr<fsreferrer> ref = self_ref.lock();
    this->inode = inode.CreateReference(ref);
}

std::shared_ptr<ext2fs_inode_reader> ext2fs_inode_reader::Create(fsresource<ext2fs_inode> &inode) {
    std::shared_ptr<ext2fs_inode_reader> reader{new ext2fs_inode_reader()};
    std::weak_ptr<ext2fs_inode_reader> weak{reader};
    reader->self_ref = weak;
    reader->Init(inode);
    return reader;
}

std::shared_ptr<ext2fs_inode_reader> ext2fs_inode_reader::Create(const fsreference<ext2fs_inode> &inode) {
    std::shared_ptr<ext2fs_inode_reader> reader{new ext2fs_inode_reader()};
    std::weak_ptr<ext2fs_inode_reader> weak{reader};
    reader->self_ref = weak;
    reader->Init(inode);
    return reader;
}

std::string ext2fs_inode_reader::GetReferrerIdentifier() {
    return "";
}

filesystem_status ext2fs_inode_reader::seek_set(std::size_t offset) {
    next_blki = offset / FILEPAGE_PAGE_SIZE;
    this->offset = offset % FILEPAGE_PAGE_SIZE;
    if (cur_blki != next_blki || !page) {
        cur_blki = next_blki;
        auto result = inode->ReadBlock(cur_blki);
        page = result.page;
        next_blki++;
        return result.status;
    } else {
        next_blki++;
        return filesystem_status::SUCCESS;
    }
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