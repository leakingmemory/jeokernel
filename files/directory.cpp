//
// Created by sigsegv on 6/5/22.
//

#include <files/directory.h>

std::shared_ptr<fileitem> directory::Resolve(std::string filename) {
    if (filename.empty() || filename.starts_with("/")) {
        return {};
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
    for (auto entry : Entries()) {
        if (component == entry->Name()) {
            if (filename.empty()) {
                return entry->Item();
            } else {
                directory *dir = dynamic_cast<directory *>(&(*(entry->Item())));
                if (dir == nullptr) {
                    return {};
                }
                return dir->Resolve(filename);
            }
        }
    }
    return {};
}