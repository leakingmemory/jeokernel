//
// Created by sigsegv on 2/10/22.
//

#include <blockdevs/mbr_reader.h>
#include <blockdevs/offset_blockdev.h>

struct MbrAddress {
    uint8_t head;
    uint8_t sector : 6;
    uint8_t cyl_hi : 2;
    uint8_t cyl_lo;

    uint16_t Cylinder() {
        uint16_t cyl{cyl_hi};
        cyl = cyl << 8;
        cyl |= cyl_lo;
        return cyl;
    }
    uint8_t Head() {
        return head;
    }
    uint8_t Sector() {
        return sector;
    }

    uint32_t LBA(uint8_t heads, uint8_t sectors) {
        uint32_t lba{Cylinder()};
        lba = lba * heads;
        lba = lba + Head();
        lba = lba * sectors;
        lba = lba + Sector() - 1;
        return lba;
    }
} __attribute__((__packed__));

struct MbrPartition {
    uint8_t status;
    MbrAddress start;
    uint8_t type;
    MbrAddress last;
    uint32_t lbaStart;
    uint32_t sizeSectors;

    uint32_t StartBlock() {
        if (lbaStart > 0) {
            return lbaStart;
        }
        return 0;
    }
    uint32_t Size() {
        if (sizeSectors > 0) {
            return sizeSectors;
        }
        return 0;
    }
} __attribute__((__packed__));

class mbr_parttable_entry : public parttable_entry {
private:
    std::size_t offset;
    std::size_t size;
    unsigned int type;
public:
    mbr_parttable_entry(std::size_t offset, std::size_t size, unsigned int type) : offset(offset), size(size), type(type) { }
    std::size_t GetOffset() override;
    std::size_t GetSize() override;
    unsigned int GetType() override;
};

std::size_t mbr_parttable_entry::GetOffset() {
    return offset;
}

std::size_t mbr_parttable_entry::GetSize() {
    return size;
}

unsigned int mbr_parttable_entry::GetType() {
    return type;
}

class mbr_parttable : public raw_parttable {
private:
    uint64_t signature;
    std::size_t blockSize;
    std::vector<std::shared_ptr<parttable_entry>> partitions;
public:
    mbr_parttable(uint64_t signature, std::size_t blockSize, MbrPartition *parts, int index, int n) : signature(signature), blockSize(blockSize), partitions() {
        for (int i = index; i < n; i++) {
            uint64_t start = parts[i].StartBlock();
            {
                start *= 512;
                start /= blockSize;
            }
            uint64_t size = parts[i].Size();
            {
                size *= 512;
                size /= blockSize;
            }
            if (start == 0 || size == 0) {
                continue;
            }
            std::shared_ptr<parttable_entry> entry{new mbr_parttable_entry(start, size, parts[i].type)};
            partitions.push_back(entry);
        }
    }

    std::string GetTableType() const override;
    uint64_t GetSignature() override;
    std::size_t GetBlockSize() override;
    std::vector<std::shared_ptr<parttable_entry>> GetEntries() override;
    void Add(std::shared_ptr<parttable_entry> entry) {
        partitions.push_back(entry);
    }
};

void ReadExtendedPartition(const mbr_reader &reader, mbr_parttable &parttable, std::shared_ptr<blockdev> up_blockdev) {
    auto entries = parttable.GetEntries();
    for (auto entry : entries) {
        if (entry->GetType() != 5 && entry->GetType() != 0xF) {
            continue;
        }
        auto offset = entry->GetOffset();
        if (offset == 0 || entry->GetSize() == 0) {
            continue;
        }

        std::shared_ptr<blockdev> off_blockdev{new offset_blockdev(up_blockdev, offset, entry->GetSize())};

        auto ext_parttable = reader.ReadParttable(off_blockdev, true);

        auto ext_parttable_entries = ext_parttable->GetEntries();

        for (auto ext_entry : ext_parttable_entries) {
            std::shared_ptr<parttable_entry> add_entry{new mbr_parttable_entry(ext_entry->GetOffset() + offset, ext_entry->GetSize(), ext_entry->GetType())};
            parttable.Add(add_entry);
        }
    }
}

std::string mbr_parttable::GetTableType() const {
    return "MBR";
}

uint64_t mbr_parttable::GetSignature() {
    return signature;
}

std::size_t mbr_parttable::GetBlockSize() {
    return blockSize;
}

std::vector<std::shared_ptr<parttable_entry>> mbr_parttable::GetEntries() {
    return partitions;
}

mbr_reader::mbr_reader() {
}

std::shared_ptr<raw_parttable> mbr_reader::ReadParttable(std::shared_ptr<blockdev> blockdev) const {
    return ReadParttable(blockdev, false);
}

std::shared_ptr<raw_parttable> mbr_reader::ReadParttable(std::shared_ptr<blockdev> blockdev, bool extendedPartition) const {
    std::size_t blocksToRead{512 / blockdev->GetBlocksize()};
    if ((512 % blockdev->GetBlocksize()) != 0) {
        ++blocksToRead;
    }

    auto mbrBlock = blockdev->ReadBlock(0, blocksToRead);

    if (!mbrBlock || mbrBlock->Size() < 512) {
        return {};
    }

    uint8_t *mbr = (uint8_t *) mbrBlock->Pointer();
    if (mbr[0x1FE] != 0x55 || mbr[0x1FF] != 0xAA) {
        return {};
    }

    uint32_t signature = *((uint32_t *) (void *) (mbr + 0x1B8));

    MbrPartition *partitions = (MbrPartition *) (void *) (mbr + 0x1BE);

    mbr_parttable *mbrParttable = new mbr_parttable(signature, blockdev->GetBlocksize(), partitions, 0, extendedPartition ? 2 : 4);

    std::shared_ptr<raw_parttable> parttable{mbrParttable};

    ReadExtendedPartition(*this, *mbrParttable, blockdev);

    return parttable;
}