//
// Created by sigsegv on 3/9/22.
//

#ifndef JEOKERNEL_STATISTICS_OBJECT_H
#define JEOKERNEL_STATISTICS_OBJECT_H

class statistics_visitor;

class statistics_object {
public:
    virtual ~statistics_object() {}
    virtual void Accept(statistics_visitor &visitor) = 0;
};

#endif //JEOKERNEL_STATISTICS_OBJECT_H
