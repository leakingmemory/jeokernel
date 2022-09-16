//
// Created by sigsegv on 6/5/22.
//

#include <files/directory.h>

directory_resolve_result directory::Resolve(std::string filename) {
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
            if (filename.empty()) {
                return {.file = entry->Item(), .status = fileitem_status::SUCCESS};
            } else {
                directory *dir = dynamic_cast<directory *>(&(*(entry->Item())));
                if (dir == nullptr) {
                    return {.file = {}, .status = fileitem_status::SUCCESS};
                }
                return dir->Resolve(filename);
            }
        }
    }
    return {.file = {}, .status = fileitem_status::SUCCESS};
}