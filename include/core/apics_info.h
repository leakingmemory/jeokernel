//
// Created by sigsegv on 12/24/21.
//

#ifndef JEOKERNEL_APICS_INFO_H
#define JEOKERNEL_APICS_INFO_H

class apics_info {
public:
    const apics_info *GetApicsInfo() const {
        return this;
    }
    virtual int GetNumberOfIoapics() const = 0;
    virtual uint64_t GetIoapicAddr(int) const = 0;
};

#endif //JEOKERNEL_APICS_INFO_H
