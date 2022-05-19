//
// Created by sigsegv on 5/17/22.
//

#include <filesystems/filesystem.h>

static std::vector<std::shared_ptr<filesystem_provider>> *providers = nullptr;

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
std::shared_ptr<blockdev_filesystem> open_filesystem(std::string provider_name, std::shared_ptr<blockdev> bdev) {
    for (auto provider : *providers) {
        if (provider_name == provider->name()) {
            return provider->open(bdev);
        }
    }
    return {};
}
