//
// Created by sigsegv on 6/8/22.
//

#include <iostream>
#include <exec/exec.h>
#include <exec/process.h>
#include <kfs/kfiles.h>
#include "UserElf.h"

#define USERSPACE_DEFAULT_STACKSIZE     ((uintptr_t) ((uintptr_t) 32*1048576))

//#define DEBUG_USERSPACE_PAGELIST

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
    constexpr uint64_t lowerUserspaceEnd = ((uint64_t) USERSPACE_LOW_END) << (9+9+12);
    constexpr uint64_t upperUserspaceStart = ((uint64_t) PMLT4_USERSPACE_HIGH_START) << (9+9+9+12);
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
        uint64_t relocationOffset;
        if (endpage <= lowerUserspaceEnd) {
            relocationOffset = 0;
        } else {
            relocationOffset = upperUserspaceStart;
        }
        std::vector<exec_pageinfo> pages{};
        pages.reserve(endpage - startpage);
        constexpr uint32_t doNotLoad = -1;
        for (auto i = startpage; i < endpage; i++) {
            pages.push_back({.filep = doNotLoad, .write = false, .exec = false});
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
            auto pvendaddr = vendaddr;
            if (pe->p_filesz < pe->p_memsz) {
                std::cerr << "Warning: Less filesize than memsize "<<pe->p_filesz << "<" << pe->p_memsz
                << " may not be correctly implemented\n";
                pvendaddr = pe->p_vaddr + pe->p_filesz;
                if ((pvendaddr & (PAGESIZE - 1)) == 0) {
                    pvendaddr = pvendaddr >> 12;
                } else {
                    pvendaddr = (pvendaddr >> 12) + 1;
                }
            }
            for (auto i = vpageaddr; i < vpageendaddr; i++) {
                auto rel = i - vpageaddr;
                auto &page = pages[i - startpage];
                if (i < pvendaddr) {
                    page.filep = ppageaddr + rel;
                } else {
                    page.filep = doNotLoad;
                }
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
        constexpr uint16_t zero = 8;

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
                if (page.filep == doNotLoad) {
                    currentFlags |= zero;
                }
                if (currentFlags != flags || (page.filep != doNotLoad && page.filep != (offset + (index - start)))) {
                    if (flags != none) {
                        bool success;
                        if ((flags & zero) == 0) {
                            success = process->Map(binary, startpage + start, index - start, offset,
                                                        (flags & write) != 0, (flags & exec) != 0, true);
                        } else {
                            success = process->Map(startpage + start, index - start);
                        }
                        if (!success) {
                            std::cerr << "Error: Map failed\n";
                        }
                    }
                    start = index;
                    flags = currentFlags;
                    offset = page.filep;
                }
                ++index;
            }
            if (flags != none) {
                bool success;
                if ((flags & zero) == 0) {
                    success = process->Map(binary, startpage + start, index - start, offset, (flags & write) != 0,
                                                (flags & exec) != 0, true);
                } else {
                    success = process->Map(startpage + start, index - start);
                }
                if (!success) {
                    std::cerr << "Error: Map failed\n";
                }
            }
        }

#ifdef DEBUG_USERSPACE_PAGELIST
        {
            int i = startpage;
            for (const auto &page: pages) {
                std::cout << "Page " << std::hex << i << " -> file:" << page.filep << std::dec << (page.write ? " write" : "") << (page.exec ? " exec" : "") << "\n";
                ++i;
            }
        }
#endif

        uint64_t entrypoint = userElf.get_entrypoint_addr();

        if (relocationOffset != 0) {
            std::cout << "Relocation " <<std::hex<< (startpage) << "->" << (relocationOffset+startpage) <<std::dec<< "\n";
            entrypoint += relocationOffset;
        }

        constexpr uintptr_t stackPages = USERSPACE_DEFAULT_STACKSIZE / PAGESIZE;
        uintptr_t stackAddr = process->FindFree(stackPages);
        if (stackAddr == 0) {
            std::cerr << "Error: Unable to allocate stack space\n";
            delete process;
            return;
        }
        if (!process->Map(stackAddr, stackPages)) {
            std::cerr << "Error: Unable to allocate stack space (map failed)\n";
            delete process;
            return;
        }
        stackAddr += stackPages;
        stackAddr = stackAddr << 12;

        std::vector<std::string> environ{};
        std::vector<std::string> argv{};
        argv.push_back(name);
        auto argc = argv.size();
        process->push_strings(stackAddr, environ, [process, argv, argc, entrypoint, cmd_name] (bool success, uintptr_t environAddr) {
            if (!success) {
                std::cerr << "Error: Failed to push environment to stack for new process\n";
                delete process;
                return;
            }
            process->push_strings(environAddr, argv, [process, environAddr, argc, entrypoint, cmd_name] (bool success, uintptr_t argvAddr) {
                if (!success) {
                    std::cerr << "Error: Failed to push args to stack for new process\n";
                    delete process;
                    return;
                }
                process->push_64(argvAddr, environAddr, [process, argvAddr, argc, entrypoint, cmd_name] (bool success, uintptr_t environpAddr) {
                    if (!success) {
                        std::cerr << "Error: Failed to push environ ptr to stack for new process\n";
                        delete process;
                        return;
                    }
                    process->push_64(environpAddr, argvAddr, [process, argc, entrypoint, cmd_name] (bool success, uintptr_t argvpAddr) {
                        if (!success) {
                            std::cerr << "Error: Failed to push args ptr to stack for new process\n";
                            delete process;
                            return;
                        }
                        process->push_64(argvpAddr, argc, [process, entrypoint, cmd_name] (bool success, uintptr_t stackAddr) {
                            if (!success) {
                                std::cerr << "Error: Failed to push args count to stack for new process\n";
                                delete process;
                                return;
                            }
                            std::vector<task_resource *> resources{};
                            auto *scheduler = get_scheduler();
                            resources.push_back(process);
                            auto pid = scheduler->new_task(
                                    entrypoint,
                                    0x18 | 3 /* ring3 / lowest*/,
                                    0x20 | 3, 0, stackAddr, 0, 0, 0,
                                    0, 0, 0, resources);
                            std::cout << "Started task " << pid << "\n";
                            scheduler->set_name(pid, cmd_name);
                        });
                    });
                });
            });
        });
    }
}
