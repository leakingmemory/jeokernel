//
// Created by sigsegv on 3/9/22.
//

#include <stats/statistics_root.h>
#include <mutex>

void statistics_root::Accept(statistics_visitor &visitor) {
    std::vector<statistics_root_entry> entries{};
    {
        std::lock_guard lock{_lock};
        for (const auto &entry : this->entries) {
            entries.push_back(entry);
        }
    }
    for (auto &entry : entries) {
        auto object = entry.factory->GetObject();
        visitor.Visit(entry.name, *object);
    }
}

void statistics_root::Add(const std::string &name, std::shared_ptr<statistics_object_factory> statistics) {
    std::lock_guard lock{_lock};
    entries.push_back({.name = name, .factory = statistics});
}

void statistics_root::Remove(std::shared_ptr<statistics_object_factory> statistics) {
    std::lock_guard lock{_lock};
    auto iter = entries.begin();
    while (iter == entries.end()) {
        if ((*iter).factory == statistics) {
            entries.erase(iter);
            return;
        }
        ++iter;
    }
}

static uint8_t rootObjStorage[sizeof(statistics_root)];
static statistics_root *root = nullptr;

statistics_root &GetStatisticsRoot() {
    if (root == nullptr) {
        root = new ((void *) &rootObjStorage) statistics_root();
    }
    return *root;
}
