//
// Created by sigsegv on 3/9/22.
//

#ifndef JEOKERNEL_STATISTICS_ROOT_H
#define JEOKERNEL_STATISTICS_ROOT_H

#include <stats/statistics_object.h>
#include <stats/statistics_visitor.h>
#include <memory>
#include <vector>
#include <concurrency/hw_spinlock.h>

class statistics_object_factory {
public:
    virtual ~statistics_object_factory() {}
    virtual std::shared_ptr<statistics_object> GetObject() = 0;
};

struct statistics_root_entry {
    std::string name;
    std::shared_ptr<statistics_object_factory> factory;
};

class statistics_root {
private:
    hw_spinlock _lock;
    std::vector<statistics_root_entry> entries;
public:
    statistics_root() : _lock(), entries() {}
    void Accept(statistics_visitor &visitor);
    void Add(const std::string &name, std::shared_ptr<statistics_object_factory> statistics);
    void Remove(std::shared_ptr<statistics_object_factory> statistics);
};

statistics_root &GetStatisticsRoot();

#endif //JEOKERNEL_STATISTICS_ROOT_H
