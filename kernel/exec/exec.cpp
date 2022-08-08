//
// Created by sigsegv on 6/8/22.
//

#include <iostream>
#include <exec/exec.h>
#include <exec/procthread.h>
#include <kfs/kfiles.h>
#include "UserElf.h"

#define USERSPACE_DEFAULT_STACKSIZE     ((uintptr_t) ((uintptr_t) 32*1048576))

//#define DEBUG_USERSPACE_PAGELIST

struct exec_pageinfo {
    uint32_t filep;
    uint16_t load;
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
    uintptr_t tlsStart{0};
    uintptr_t tlsMemSize{0};
    uintptr_t tlsAlign{0};
    uintptr_t fsBase{0};
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
            if (pe->p_type == PHT_TLS) {
                tlsStart = pe->p_vaddr;
                tlsMemSize = pe->p_memsz;
                tlsAlign = pe->p_align;
            }
        }
        fsBase = tlsStart + tlsMemSize;
        if (tlsAlign != 0) {
            auto fsBaseOff = fsBase % tlsAlign;
            if (fsBaseOff != 0) {
                fsBase += tlsAlign;
            }
            fsBase -= fsBaseOff;
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
            pages.push_back({.filep = doNotLoad, .load = 0, .write = false, .exec = false});
        }
        uintptr_t program_brk{0};
        for (auto &pe : loads) {
            auto vpageaddr = pe->p_vaddr >> 12;
            auto vpageoff = pe->p_vaddr & (PAGESIZE - 1);
            auto ppageaddr = pe->p_offset >> 12;
            auto ppageoff = pe->p_offset & (PAGESIZE - 1);
            if (vpageoff != ppageoff) {
                std::cerr << "Error: Misalignment in ELF file prevents loading by page\n";
            }
            auto vpageendaddr = pe->p_vaddr + pe->p_memsz;
            if (vpageendaddr > program_brk) {
                program_brk = vpageendaddr;
            }
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
            }
            for (auto i = vpageaddr; i < vpageendaddr; i++) {
                auto rel = i - vpageaddr;
                auto &page = pages[i - startpage];
                uintptr_t addr{i};
                addr = addr << 12;
                if (addr < pvendaddr) {
                    page.filep = ppageaddr + rel;
                    if ((pvendaddr - addr) < PAGESIZE) {
                        page.load = pvendaddr - addr;
                    }
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

        auto *process = new ProcThread();
        process->SetProgramBreak(program_brk);
        {
            uint32_t index = 0;
            uint32_t start = 0;
            uint32_t flags = none;
            uint32_t offset = 0;
            uint16_t load = 0;
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
                if (currentFlags != flags || load != 0 || page.load != 0 || (page.filep != doNotLoad && page.filep != (offset + (index - start)))) {
                    if (flags != none) {
                        if (load != 0 && (index - start) != 1) {
                            wild_panic("Invalid load!=0 && len>1");
                        }
                        bool success;
                        if ((flags & zero) == 0) {
                            success = process->Map(binary, startpage + start, index - start, offset, load,
                                                        (flags & write) != 0, (flags & exec) != 0, true, true);
                        } else {
                            success = process->Map(startpage + start, index - start, true);
                        }
                        if (!success) {
                            std::cerr << "Error: Map failed\n";
                        }
                    }
                    start = index;
                    flags = currentFlags;
                    offset = page.filep;
                }
                load = page.load;
                ++index;
            }
            if (flags != none) {
                bool success;
                if ((flags & zero) == 0) {
                    if (load != 0 && (index - start) != 1) {
                        wild_panic("Invalid load!=0 && len>1");
                    }
                    success = process->Map(binary, startpage + start, index - start, offset, load, (flags & write) != 0,
                                                (flags & exec) != 0, true, true);
                } else {
                    success = process->Map(startpage + start, index - start, true);
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
        if (!process->Map(stackAddr, stackPages, false)) {
            std::cerr << "Error: Unable to allocate stack space (map failed)\n";
            delete process;
            return;
        }
        stackAddr += stackPages;
        stackAddr = stackAddr << 12;

        std::shared_ptr<std::vector<std::string>> environ{new std::vector<std::string>};
        std::shared_ptr<std::vector<std::string>> argv{new std::vector<std::string>};
        argv->push_back(name);
        auto argc = argv->size();
        typedef struct {
            uint8_t data[16];
        } random_data_t;
        std::shared_ptr<random_data_t> random{new random_data_t};
        {
            // TODO - random data hardcoded
            uint8_t random_data[16] = {13, 0xFE, 27, 59, 46, 33, 134, 201, 101, 178, 199, 3, 99, 8, 144, 177};
            memcpy(&(random->data[0]), &(random_data[0]), sizeof(random->data));
        }
        process->push_data(stackAddr, &(random->data[0]), sizeof(random->data), [process, random, environ, argv, argc, entrypoint, cmd_name, fsBase] (bool success, uintptr_t randomAddr) {
            if (!success) {
                std::cerr << "Error: Failed to push random data to stack for new process\n";
                delete process;
                return;
            }
            std::shared_ptr<std::vector<ELF64_auxv>> auxv{new std::vector<ELF64_auxv>};
            auxv->push_back({.type = AT_RANDOM, .uintptr = randomAddr});
            process->push_strings(randomAddr, environ->begin(), environ->end(), std::vector<uintptr_t>(), [process, environ, argv, argc, auxv, entrypoint, cmd_name, fsBase] (bool success, const std::vector<uintptr_t> ptrs, uintptr_t stackAddr) {
                if (!success) {
                    std::cerr << "Error: Failed to push environment to stack for new process\n";
                    delete process;
                    return;
                }
                std::vector<uintptr_t> environPtrs{ptrs};
                process->push_strings(stackAddr, argv->begin(), argv->end(), std::vector<uintptr_t>(), [process, environPtrs, argv, argc, auxv, entrypoint, cmd_name, fsBase] (bool success, const std::vector<uintptr_t> &ptrs, uintptr_t stackAddr) mutable {
                    if (!success) {
                        std::cerr << "Error: Failed to push args to stack for new process\n";
                        delete process;
                        return;
                    }
                    std::vector<uintptr_t> argvPtrs{ptrs};
                    process->push_64(stackAddr, 0, [process, environPtrs, argvPtrs, argc, auxv, entrypoint, cmd_name, fsBase] (bool success, uintptr_t stackAddr) mutable {
                        if (!success) {
                            std::cerr << "Error: Failed to push end of auxv to stack for new process\n";
                            delete process;
                            return;
                        }
                        process->push_data(stackAddr, &(auxv->at(0)), sizeof(auxv->at(0)) * auxv->size(), [process, auxv, environPtrs, argvPtrs, argc, entrypoint, cmd_name, fsBase] (bool success, uintptr_t auxvAddr) mutable {
                            if (!success) {
                                std::cerr << "Error: Failed to push auxv to stack for new process\n";
                                delete process;
                                return;
                            }
                            std::shared_ptr<std::vector<uintptr_t>> environ{new std::vector<uintptr_t>(environPtrs)};
                            environ->push_back(0);
                            process->push_data(auxvAddr, &(environ->at(0)), sizeof(environ->at(0)) * environ->size(), [process, environ, argvPtrs, argc, auxvAddr, entrypoint, cmd_name, fsBase] (bool success, uintptr_t environAddr) {
                                if (!success) {
                                    std::cerr << "Error: Failed to push environment to stack for new process\n";
                                    delete process;
                                    return;
                                }
                                std::shared_ptr<std::vector<uintptr_t>> argv{new std::vector<uintptr_t>(argvPtrs)};
                                process->push_data(environAddr, &(argv->at(0)), sizeof(argv->at(0)) * argv->size(), [process, argv, auxvAddr, environAddr, argc, entrypoint, cmd_name, fsBase] (bool success, uintptr_t argvAddr) {
                                    if (!success) {
                                        std::cerr << "Error: Failed to push args to stack for new process\n";
                                        delete process;
                                        return;
                                    }
                                    process->push_64(argvAddr, environAddr, [process, argvAddr, argc, entrypoint, cmd_name, fsBase] (bool success, uintptr_t stackAddr) {
                                        if (!success) {
                                            std::cerr << "Error: Failed to push environ ptr to stack for new process\n";
                                            delete process;
                                            return;
                                        }
                                        process->push_64(stackAddr, argvAddr, [process, argc, entrypoint, cmd_name, fsBase] (bool success, uintptr_t stackAddr) {
                                            if (!success) {
                                                std::cerr << "Error: Failed to push args ptr to stack for new process\n";
                                                delete process;
                                                return;
                                            }
                                            process->push_64(stackAddr, argc, [process, entrypoint, cmd_name, fsBase] (bool success, uintptr_t stackAddr) {
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
                                                        0x20 | 3, fsBase, 0, stackAddr, 0, 0, 0,
                                                        0, 0, 0, resources);
                                                std::cout << "Started task " << pid << "\n";
                                                scheduler->set_name(pid, cmd_name);
                                            });
                                        });
                                    });
                                });
                            });
                        });
                    });
                });
            });
        });
    }
}
