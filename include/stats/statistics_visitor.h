//
// Created by sigsegv on 3/9/22.
//

#ifndef JEOKERNEL_STATISTICS_VISITOR_H
#define JEOKERNEL_STATISTICS_VISITOR_H

#include <string>

class statistics_object;

class statistics_visitor {
public:
    virtual ~statistics_visitor() {}
    virtual void Visit(const std::string &name, statistics_object &object) = 0;
    virtual void Visit(const std::string &name, long long int value) = 0;
};

#endif //JEOKERNEL_STATISTICS_VISITOR_H
