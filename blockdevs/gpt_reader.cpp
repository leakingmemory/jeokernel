//
// Created by sigsegv on 2/20/22.
//

#include <blockdevs/gpt_reader.h>
#include <hashfunc/crc32.h>
#include <cstring>

struct GptGuid {
    uint16_t guid[8];
} __attribute__((__packed__));
static_assert(sizeof(GptGuid) == 16);

struct GptHeader {
    uint64_t signature;
    uint32_t revision;
    uint32_t headerSize;
    uint32_t headerCrc32;
    uint32_t reserved;
    uint64_t currentLba;
    uint64_t backupLba;
    uint64_t firstUsableLba;
    uint64_t lastUsableLba;
    GptGuid diskGuid;
    uint64_t startingLbaOfPartitionTable;
    uint32_t numPartitions;
    uint32_t sizeOfPartitionEntry;
    uint32_t partitionTableCrc32;

    bool IsGptSignature() const {
        return signature == 0x5452415020494645;
    }
} __attribute__((__packed__));

struct GptPartEntry {
    GptGuid partitionTypeGuid;
    GptGuid partitionGuid;
    uint64_t firstLba;
    uint64_t lastLba;
    uint64_t flags;
    uint16_t name[36];
} __attribute__((__packed__));

class gpt_header_container {
private:
    std::size_t size;
    void *buf;
    GptHeader *header;
public:
    gpt_header_container() : size(sizeof(*header)), buf(malloc(size)), header((GptHeader *) buf) {
    }
    ~gpt_header_container();
    gpt_header_container(const gpt_header_container &cp);
    gpt_header_container(gpt_header_container &&mv);
    gpt_header_container &operator =(const gpt_header_container &cp);
    gpt_header_container &operator =(gpt_header_container &&mv);

    void Resize(std::size_t newSize);

    [[nodiscard]] const GptHeader &ConstHeader() const {
        return *header;
    }
    GptHeader &Header() {
        return *header;
    }
    void *Buf() {
        return buf;
    }
    std::size_t Size() const {
        return size;
    }
};

gpt_header_container::~gpt_header_container() {
    if (buf != nullptr) {
        free(buf);
        buf = nullptr;
    }
}

gpt_header_container::gpt_header_container(const gpt_header_container &cp) : size(cp.size), buf(nullptr), header(nullptr) {
    if (cp.buf != nullptr) {
        buf = malloc(size);
        memcpy(buf, cp.buf, size);
        header = (GptHeader *) buf;
    } else {
        header = cp.header;
    }
}

gpt_header_container::gpt_header_container(gpt_header_container &&mv) : size(mv.size), buf(mv.buf), header(mv.header) {
    mv.buf = nullptr;
}

gpt_header_container &gpt_header_container::operator=(const gpt_header_container &cp) {
    if (cp.buf != nullptr) {
        if (buf == nullptr || size < cp.size) {
            if (buf != nullptr) {
                free(buf);
            }
            size = cp.size;
            buf = malloc(size);
            memcpy(buf, cp.buf, size);
            header = (GptHeader *) buf;
        } else {
            if (size > cp.size) {
                bzero(buf, size);
            }
            memcpy(buf, cp.buf, cp.size);
        }
    } else {
        if (this == &cp) {
            return *this;
        }
        if (buf != nullptr) {
            if (cp.header != nullptr) {
                bzero(buf, size);
                memcpy(buf, cp.header, sizeof(*(cp.header)));
                header = (GptHeader *) buf;
            } else {
                free(buf);
                buf = nullptr;
                header = nullptr;
            }
        } else {
            size = cp.size;
            buf = cp.buf;
            header = cp.header;
        }
    }
    return *this;
}

gpt_header_container &gpt_header_container::operator=(gpt_header_container &&mv) {
    if (this == &mv) {
        return *this;
    }
    if (buf != nullptr) {
        free(buf);
    }
    size = mv.size;
    buf = mv.buf;
    header = mv.header;
    mv.buf = nullptr;
    return *this;
}

void gpt_header_container::Resize(std::size_t newSize) {
    if (newSize < sizeof(*header)) {
        newSize = sizeof(*header);
    }
    if (buf != nullptr) {
        if (newSize == size) {
            return;
        }
        void *newBuf = realloc(buf, newSize);
        if (newBuf != nullptr) {
            buf = newBuf;
            size = newSize;
            header = (GptHeader *) buf;
        }
    } else {
        buf = malloc(newSize);
        size = newSize;
        bzero(buf, size);
        header = (GptHeader *) buf;
    }
}

template <class Entry> class gpt_table_container {
private:
    void *buf;
    std::size_t size;
    std::size_t entrySize;
    std::size_t numEntries;
public:
    gpt_table_container() : buf(nullptr), size(0), entrySize(0), numEntries(0) {
    }
    gpt_table_container(const void *input, std::size_t size, std::size_t entrySize, std::size_t numEntries)
    : buf(malloc(size)), size(size), entrySize(entrySize), numEntries(numEntries) {
        if (this->entrySize < sizeof(Entry)) {
            this->numEntries = 0;
        }
        if ((this->numEntries * this->entrySize) > this->size) {
            this->numEntries = this->size / this->entrySize;
        }
        memcpy(buf, input, this->size);
    }
    ~gpt_table_container() {
        if (buf != nullptr) {
            free(buf);
            buf = nullptr;
        }
    }

private:
    template <class T> void Swap(T &one, T &other) {
        T tmp{one};
        one = other;
        other = tmp;
    }

    void Swap(gpt_table_container &other) {
        Swap(this->buf, other.buf);
        Swap(this->size, other.size);
        Swap(this->entrySize, other.entrySize);
        Swap(this->numEntries, other.numEntries);
    }
public:

    gpt_table_container(const gpt_table_container &cp) : buf(malloc(cp.size)), size(cp.size), entrySize(cp.entrySize), numEntries(cp.numEntries) {
        memcpy(buf, cp.buf, size);
    }
    gpt_table_container(gpt_table_container &&mv) : buf(mv.buf), size(mv.size), entrySize(mv.entrySize), numEntries(mv.numEntries) {
        mv.buf = nullptr;
    }
    gpt_table_container &operator =(const gpt_table_container &cp) {
        gpt_table_container copy{cp};
        Swap(copy);
        return *this;
    }
    gpt_table_container &operator =(gpt_table_container &&mv) {
        Swap(mv);
        return *this;
    }

    std::size_t NumEntries() const {
        return numEntries;
    }
    const Entry &GetEntry(std::size_t index) const {
        uint8_t *ptr = (uint8_t *) buf;
        {
            std::size_t offset{index * entrySize};
            ptr += offset;
        }
        return *((Entry *) (void *) ptr);
    }

    uint32_t Crc32() const {
        crc32 crc{};
        crc.byte_hashfunc::encode((const void *) buf, size);
        return crc.complete();
    }
};

class gpt_parttable_entry : public parttable_entry {
private:
    std::size_t offset;
    std::size_t size;
public:
    gpt_parttable_entry(std::size_t offset, std::size_t size) : offset(offset), size(size) { }
    std::size_t GetOffset() override;
    std::size_t GetSize() override;
    unsigned int GetType() override;
};

std::size_t gpt_parttable_entry::GetOffset() {
    return offset;
}

std::size_t gpt_parttable_entry::GetSize() {
    return size;
}

unsigned int gpt_parttable_entry::GetType() {
    return 0;
}

class gpt_parttable : public raw_parttable {
private:
    std::size_t blockSize;
    std::vector<std::shared_ptr<parttable_entry>> partitions;
public:
    gpt_parttable(const GptHeader &header, std::size_t sectorSize, std::size_t blockSize, const gpt_table_container<GptPartEntry> &table);
    uint64_t GetSignature() override;
    std::size_t GetBlockSize() override;
    std::vector<std::shared_ptr<parttable_entry>> GetEntries() override;
};

gpt_parttable::gpt_parttable(const GptHeader &header, std::size_t sectorSize, std::size_t blockSize,
                             const gpt_table_container<GptPartEntry> &table)
     : blockSize(blockSize), partitions() {
    for (std::size_t i = 0; i < table.NumEntries(); i++) {
        const auto &entry = table.GetEntry(i);
        if (entry.firstLba == 0 || entry.lastLba == 0) {
            continue;
        }
        uint64_t blocks_offset{entry.firstLba};
        blocks_offset *= sectorSize;
        blocks_offset /= blockSize;
        uint64_t blocks_size{entry.lastLba - entry.firstLba + 1};
        blocks_size *= sectorSize;
        blocks_size /= blockSize;
        std::shared_ptr<parttable_entry> pt_entry{new gpt_parttable_entry(blocks_offset, blocks_size)};
        partitions.push_back(pt_entry);
    }
}

uint64_t gpt_parttable::GetSignature() {
    return 0;
}

std::size_t gpt_parttable::GetBlockSize() {
    return blockSize;
}

std::vector<std::shared_ptr<parttable_entry>> gpt_parttable::GetEntries() {
    return partitions;
}

gpt_reader::gpt_reader() {
}

std::shared_ptr<raw_parttable> gpt_reader::ReadParttable(std::shared_ptr<blockdev> blockdev) const {
    std::size_t sectorSize{512};

    gpt_header_container hdr_container{};

    std::size_t lastgptblock{0};
    {
        std::size_t gptoffset = sectorSize;
        std::size_t gptsize = sizeof(GptHeader);
        std::size_t gptblock = gptoffset / blockdev->GetBlocksize();
        lastgptblock = (gptoffset + gptsize - 1) / blockdev->GetBlocksize();
        gptoffset = gptoffset % blockdev->GetBlocksize();

        hdr_container.Resize(((lastgptblock - gptblock + 1) * blockdev->GetBlocksize()) - gptoffset);

        {
            auto blocks = blockdev->ReadBlock(gptblock, lastgptblock - gptblock + 1);
            if (!blocks || blocks->Size() < hdr_container.Size()) {
                return {};
            }
            memcpy(hdr_container.Buf(), ((const uint8_t *) blocks->Pointer()) + gptoffset, hdr_container.Size());
        }
    }

    if (!hdr_container.ConstHeader().IsGptSignature()) {
        return {};
    }

    /* Read full length if length of gpt header is more that we've read */
    std::size_t firstread{hdr_container.Size()};
    if (firstread < hdr_container.ConstHeader().headerSize) {
        uint32_t remainingSize = hdr_container.ConstHeader().headerSize - firstread;
        uint32_t remainingBlocks = (remainingSize % blockdev->GetBlocksize()) > 0 ? 1 : 0;
        remainingBlocks += remainingSize / blockdev->GetBlocksize();

        std::size_t readSize{remainingBlocks * blockdev->GetBlocksize()};

        hdr_container.Resize(firstread + readSize);

        auto blocks = blockdev->ReadBlock(lastgptblock + 1, remainingBlocks);

        memcpy(((uint8_t *) hdr_container.Buf()) + firstread, blocks->Pointer(), readSize);
    }

    {
        uint32_t hdrcrc32{hdr_container.ConstHeader().headerCrc32};
        hdr_container.Header().headerCrc32 = 0;
        crc32 crc{};
        crc.byte_hashfunc::encode(hdr_container.Buf(), hdr_container.ConstHeader().headerSize);
        hdr_container.Header().headerCrc32 = hdrcrc32;
        if (crc.complete() != hdrcrc32) {
            return {};
        }
    }

    gpt_table_container<GptPartEntry> partTable;
    {
        std::size_t partoffset;
        std::size_t partoffset_end;
        std::size_t parttable_size;
        {
            auto header = hdr_container.ConstHeader();
            partoffset = header.startingLbaOfPartitionTable * sectorSize;
            parttable_size = header.numPartitions * header.sizeOfPartitionEntry;
            partoffset_end = partoffset + parttable_size - 1;
            std::size_t part_start{partoffset / blockdev->GetBlocksize()};
            std::size_t part_end{partoffset_end / blockdev->GetBlocksize()};
            partoffset = partoffset % blockdev->GetBlocksize();
            std::size_t parttable_read_size = (part_end - part_start + 1) * blockdev->GetBlocksize();
            {
                auto blocks = blockdev->ReadBlock(part_start, part_end - part_start + 1);

                if (!blocks || blocks->Size() < (parttable_size + partoffset)) {
                    return {};
                }

                gpt_table_container<GptPartEntry> partTableRead{
                        (const void *) (((const uint8_t *) blocks->Pointer()) + partoffset),
                        parttable_size, header.sizeOfPartitionEntry, header.numPartitions};

                partTable = std::move(partTableRead);
            }
        }

        if (partTable.Crc32() != hdr_container.ConstHeader().partitionTableCrc32) {
            return {};
        }
    }

    std::shared_ptr<raw_parttable> pt{new gpt_parttable(hdr_container.ConstHeader(), sectorSize, blockdev->GetBlocksize(), partTable)};
    return pt;
}
