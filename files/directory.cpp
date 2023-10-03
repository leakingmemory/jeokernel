//
// Created by sigsegv on 6/5/22.
//

#include <files/directory.h>
#include <files/symlink.h>

directory_resolve_result directory::Resolve(std::string filename, directory *rootdir, int followSymlink) {
    if (filename.empty() || filename.starts_with("/")) {
        return {.file = {}, .status = fileitem_status::SUCCESS};
    }
    std::string component{};
    {
        std::size_t count{0};
        {
            auto iterator = filename.begin();
            while (iterator != filename.end() && *iterator != '/') {
                ++count;
                ++iterator;
            }
            component.append(filename.c_str(), count);
            while (iterator != filename.end() && *iterator == '/') {
                ++count;
                ++iterator;
            }
        }
        std::string remaining{};
        remaining.append(filename.c_str() + count);
        filename = remaining;
    }
    auto entriesResult = Entries();
    if (entriesResult.status != fileitem_status::SUCCESS) {
        return {.file = {}, .status = entriesResult.status};
    }
    for (auto entry : entriesResult.entries) {
        if (component == entry->Name()) {
            std::shared_ptr<fileitem> fileItem{entry->Item()};
            if (followSymlink > 0) {
                auto *item = &(*(fileItem));
                class symlink *syml = dynamic_cast<class symlink *>(item);
                while (syml != nullptr && followSymlink > 0) {
                    --followSymlink;
                    std::string link = syml->GetLink();
                    if (link.starts_with("/")) {
                        if (rootdir == nullptr) {
                            return {.file = {}, .status = fileitem_status::SUCCESS};
                        }
                        std::string remaining{};
                        do {
                            remaining.append(link.c_str() + 1, link.size() - 1);
                            link = remaining;
                            remaining.clear();
                        } while (link.starts_with("/"));
                        auto result = rootdir->Resolve(link, nullptr, 0);
                        if (result.status != fileitem_status::SUCCESS || !result.file) {
                            return result;
                        }
                        fileItem = result.file;
                    } else {
                        auto result = Resolve(link, nullptr, 0);
                        if (result.status != fileitem_status::SUCCESS || !result.file) {
                            return result;
                        }
                        fileItem = result.file;
                    }
                    item = &(*fileItem);
                    syml = dynamic_cast<class symlink *>(item);
                }
                if (syml != nullptr) {
                    return {.file = {}, .status = fileitem_status::TOO_MANY_LINKS};
                }
            }
            if (filename.empty()) {
                return {.file = fileItem, .status = fileitem_status::SUCCESS};
            } else {
                auto *item = &(*(fileItem));
                directory *dir = dynamic_cast<directory *>(item);
                if (dir == nullptr) {
                    return {.file = {}, .status = fileitem_status::SUCCESS};
                }
                return dir->Resolve(filename);
            }
        }
    }
    return {.file = {}, .status = fileitem_status::SUCCESS};
}

directory_resolve_result directory::Create(std::string filename) {
    return {.file = {}, .status = fileitem_status::NOT_SUPPORTED_FS_FEATURE};
}