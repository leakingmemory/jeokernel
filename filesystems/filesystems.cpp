//
// Created by sigsegv on 5/17/22.
//

#include <filesystems/filesystem.h>

static std::vector<std::shared_ptr<filesystem_provider>> *providers = nullptr;

std::string text(filesystem_status status) {
    switch (status) {
        case filesystem_status::SUCCESS:
            return "Success";
        case filesystem_status::IO_ERROR:
            return "Input/output error";
        case filesystem_status::INTEGRITY_ERROR:
            return "Filesystem itegrity error";
        case filesystem_status::NOT_SUPPORTED_FS_FEATURE:
            return "Feature not supported";
        case filesystem_status::INVALID_REQUEST:
            return "Invalid request to filesystem";
        default:
            return "Invalid status code";
    }
}

void init_filesystem_providers() {
    providers = new std::vector<std::shared_ptr<filesystem_provider>>();
}
void add_filesystem_provider(std::shared_ptr<filesystem_provider> provider) {
    providers->push_back(provider);
}
std::vector<std::string> get_filesystem_providers() {
    std::vector<std::string> provider_names{};
    for (auto provider : *providers) {
        provider_names.push_back(provider->name());
    }
    return provider_names;
}

std::shared_ptr<filesystem> open_filesystem(std::string provider_name) {
    for (auto provider : *providers) {
        if (provider_name == provider->name()) {
            auto *special_fs = dynamic_cast<special_filesystem_provider *>(&(*provider));
            if (special_fs != nullptr) {
                return special_fs->open();
            }
        }
    }
    return {};
}

std::shared_ptr<blockdev_filesystem> open_filesystem(const std::shared_ptr<fsresourcelockfactory> &lockfactory, std::string provider_name, std::shared_ptr<blockdev> bdev) {
    for (auto provider : *providers) {
        if (provider_name == provider->name()) {
            auto *blockdev_fs = dynamic_cast<blockdev_filesystem_provider *>(&(*provider));
            if (blockdev_fs != nullptr) {
                return blockdev_fs->open(bdev, lockfactory);
            }
        }
    }
    return {};
}
