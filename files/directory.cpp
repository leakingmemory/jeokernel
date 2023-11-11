//
// Created by sigsegv on 6/5/22.
//

#include <files/directory.h>
#include <files/symlink.h>
#include <files/fsreferrer.h>
#include <sstream>

class directory_path_resolver : public fsreferrer {
private:
    directory &dir;
    std::weak_ptr<directory_path_resolver> selfRef{};
private:
    directory_path_resolver(directory &dir) : fsreferrer("directory_path_resolver"), dir(dir) {}
public:
    static std::shared_ptr<directory_path_resolver> Create(directory &dir);
    directory_path_resolver(const directory_path_resolver &) = delete;
    directory_path_resolver(directory_path_resolver &&) = delete;
    directory_path_resolver &operator =(const directory_path_resolver &) = delete;
    directory_path_resolver &operator =(directory_path_resolver &&) = delete;
    std::string GetReferrerIdentifier() override;
    directory_resolve_result Resolve(const std::shared_ptr<fsreferrer> &referrer, std::string filename, directory *rootdir, int followSymlink);
};

std::shared_ptr<directory_path_resolver> directory_path_resolver::Create(directory &dir) {
    std::shared_ptr<directory_path_resolver> obj{new directory_path_resolver(dir)};
    std::weak_ptr<directory_path_resolver> w{obj};
    obj->selfRef = w;
    return obj;
}

std::string directory_path_resolver::GetReferrerIdentifier() {
    std::stringstream id{};
    id << "directory:";
    id << dir.InodeNum();
    return id.str();
}

directory_resolve_result directory_path_resolver::Resolve(const std::shared_ptr<fsreferrer> &referrer, std::string filename, directory *rootdir, int followSymlink) {
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
    auto entriesResult = dir.Entries();
    if (entriesResult.status != fileitem_status::SUCCESS) {
        return {.file = {}, .status = entriesResult.status};
    }
    for (auto entry : entriesResult.entries) {
        if (component == entry->Name()) {
            fsreference<fileitem> fileItem{};
            {
                auto result = entry->LoadItem(selfRef.lock());
                if (result.status != fileitem_status::SUCCESS) {
                    return {.file = {}, .status = result.status};
                }
                fileItem = std::move(result.file);
                if (!fileItem) {
                    return {.file = {}, .status = fileitem_status::INTEGRITY_ERROR};
                }
            }
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
                        auto result = rootdir->Resolve(selfRef.lock(), nullptr, 0);
                        if (result.status != fileitem_status::SUCCESS || !result.file) {
                            return result;
                        }
                        fileItem = std::move(result.file);
                    } else {
                        auto result = dir.Resolve(selfRef.lock(), link, nullptr, 0);
                        if (result.status != fileitem_status::SUCCESS || !result.file) {
                            return result;
                        }
                        fileItem = std::move(result.file);
                    }
                    item = &(*fileItem);
                    syml = dynamic_cast<class symlink *>(item);
                }
                if (syml != nullptr) {
                    return {.file = {}, .status = fileitem_status::TOO_MANY_LINKS};
                }
            }
            if (filename.empty()) {
                auto ref = fileItem.CreateReference(referrer);
                return {.file = std::move(ref), .status = fileitem_status::SUCCESS};
            } else {
                auto *item = &(*(fileItem));
                directory *dir = dynamic_cast<directory *>(item);
                if (dir == nullptr) {
                    return {.file = {}, .status = fileitem_status::SUCCESS};
                }
                return dir->Resolve(referrer, filename);
            }
        }
    }
    return {.file = {}, .status = fileitem_status::SUCCESS};
}

directory_resolve_result directory::Resolve(const std::shared_ptr<fsreferrer> &referrer, std::string filename, directory *rootdir, int followSymlink) {
    std::shared_ptr<directory_path_resolver> resolver = directory_path_resolver::Create(*this);
    return resolver->Resolve(referrer, filename, rootdir, followSymlink);
}

directory_resolve_result directory::CreateFile(const std::shared_ptr<fsreferrer> &, std::string filename, uint16_t mode) {
    return {.file = {}, .status = fileitem_status::NOT_SUPPORTED_FS_FEATURE};
}

directory_resolve_result directory::CreateDirectory(const std::shared_ptr<fsreferrer> &, std::string filename, uint16_t mode) {
    return {.file = {}, .status = fileitem_status::NOT_SUPPORTED_FS_FEATURE};
}