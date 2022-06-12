//
// Created by sigsegv on 6/8/22.
//

#include <iostream>
#include <exec/exec.h>
#include <exec/process.h>
#include <kfs/kfiles.h>
#include "UserElf.h"

struct exec_pageinfo {
    uint32_t filep;
    bool write;
    bool exec;
};

void Exec::Run() {
    std::string cmd_name = name;
    UserElf userElf{binary};
    if (!userElf.is_valid()) {
        std::cerr << "Not valid ELF64\n";
        return;
    }
    uint64_t relocationOffset = ((uint64_t) PMLT4_USERSPACE_START) << (9+9+9+12);
    {
        std::vector<std::shared_ptr<ELF64_program_entry>> loads{};
        uintptr_t start = (uintptr_t) ((intptr_t) -1);
        uintptr_t end = 0;
        for (int i = 0; i < userElf.get_num_program_entries(); i++) {
            auto pe = userElf.get_program_entry(i);
            if (pe->p_type == PHT_LOAD) {
                std::cout << "Load " << std::hex << pe->p_offset << " -> " << pe->p_vaddr << " file/mem "
                          << pe->p_filesz << "/" << pe->p_memsz << "\n";
                loads.push_back(pe);
                if (pe->p_vaddr < start) {
                    start = pe->p_vaddr;
                }
                if (end < (pe->p_vaddr + pe->p_memsz)) {
                    end = pe->p_vaddr + pe->p_memsz;
                }
            }
        }
        auto startpage = start >> 12;
        auto endpage = end >> 12;
        if ((end & (PAGESIZE - 1)) != 0) {
            ++endpage;
        }
        std::vector<exec_pageinfo> pages{};
        pages.reserve(endpage - startpage);
        for (auto i = startpage; i < endpage; i++) {
            pages.push_back({.filep = 0, .write = false, .exec = false});
        }
        for (auto &pe : loads) {
            auto vpageaddr = pe->p_vaddr >> 12;
            auto vpageoff = pe->p_vaddr & (PAGESIZE - 1);
            auto ppageaddr = pe->p_offset >> 12;
            auto ppageoff = pe->p_offset & (PAGESIZE - 1);
            if (vpageoff != ppageoff) {
                std::cerr << "Error: Misalignment in ELF file prevents loading by page\n";
            }
            auto vpageendaddr = pe->p_vaddr + pe->p_memsz;
            if ((vpageendaddr & (PAGESIZE - 1)) == 0) {
                vpageendaddr = vpageendaddr >> 12;
            } else {
                vpageendaddr = (vpageendaddr >> 12) + 1;
            }
            auto vendaddr = vpageendaddr << 12;
            if (pe->p_filesz < pe->p_memsz) {
                std::cerr << "Warning: Less filesize than memsize "<<pe->p_filesz << "<" << pe->p_memsz
                << " may not be correctly implemented\n";
            }
            for (auto i = vpageaddr; i < vpageendaddr; i++) {
                auto rel = i - vpageaddr;
                auto &page = pages[i - startpage];
                page.filep = ppageaddr + rel;
            }
        }
        for (int i = 0; i < userElf.get_num_section_entries(); i++) {
            auto se = userElf.get_section_entry(i);
            auto vaddr = se->sh_addr;
            auto vsize = se->sh_size;
            if (vaddr != 0 && vsize != 0) {
                auto vend = vaddr + vsize;
                vaddr = vaddr & ~(PAGESIZE - 1);
                for (auto va = vaddr; va < vend; va += PAGESIZE) {
                    auto page = va >> 12;
                    if (page < startpage) {
                        std::cerr << "Error: ELF section pageaddr " << std::hex << page << std::dec << " out of bounds\n";
                        continue;
                    }
                    if (page >= endpage) {
                        std::cerr << "Error: ELF section pageaddr " << std::hex << page << std::dec << " out of bounds\n";
                        break;
                    }
                    if ((se->sh_flags & SHF_WRITE) != 0) {
                        std::cout << "Set write on " << std::hex << page << std::dec << "\n";
                        pages[page - startpage].write = true;
                    }
                    if ((se->sh_flags & SHF_EXECINSTR) != 0) {
                        std::cout << "Set exec on " << std::hex << page << std::dec << "\n";
                        pages[page - startpage].exec = true;
                    }
                }
            }
        }

        constexpr uint16_t none = 0;
        constexpr uint16_t read = 1;
        constexpr uint16_t write = 2;
        constexpr uint16_t exec = 4;

        Process *process = new Process();
        {
            uint32_t index = 0;
            uint32_t start = 0;
            uint32_t flags = none;
            uint32_t offset = 0;
            for (auto &page: pages) {
                uint32_t currentFlags = read;
                if (page.write) {
                    currentFlags |= write;
                }
                if (page.exec) {
                    currentFlags |= exec;
                }
                if (currentFlags != flags || page.filep != (offset + (index - start))) {
                    if (flags != none && index != start) {
                        process->Map(binary, startpage + start, index - start, offset, (flags & write) != 0, (flags & exec) != 0, true);
                    }
                    start = index;
                    flags = currentFlags;
                    offset = page.filep;
                }
                ++index;
            }
            if (flags != none && index != start) {
                process->Map(binary, startpage + start, index - start, offset, (flags & write) != 0, (flags & exec) != 0, true);
            }
        }

        {
            int i = startpage;
            for (const auto &page: pages) {
                std::cout << "Page " << std::hex << (startpage+i) << " -> file:" << page.filep << std::dec << (page.write ? " write" : "") << (page.exec ? " exec" : "") << "\n";
                ++i;
            }
        }

        uint64_t entrypoint = userElf.get_entrypoint_addr();

        if (relocationOffset != 0) {
            std::cout << "Relocation " <<std::hex<< (startpage) << "->" << (relocationOffset+startpage) <<std::dec<< "\n";
            entrypoint += relocationOffset;
        }

        std::vector<task_resource *> resources{};
        auto *scheduler = get_scheduler();
        resources.push_back(process);
        auto pid = scheduler->new_task(entrypoint, 0x18 | 3 /* ring3 / lowest*/, 0x20 | 3, 0, 0, 0, 0, 0, 0, 0, 0, resources);
        std::cout << "Started task " << pid << "\n";
        scheduler->set_name(pid, cmd_name);
    }
}
