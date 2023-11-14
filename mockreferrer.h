//
// Created by sigsegv on 11/14/23.
//

#ifndef FSBITS_MOCKREFERRER_H
#define FSBITS_MOCKREFERRER_H

#include <files/fsreferrer.h>

class mockreferrer : public fsreferrer {
public:
    mockreferrer() : fsreferrer("mockreferrer") {}
    std::string GetReferrerIdentifier() override;
};


#endif //FSBITS_MOCKREFERRER_H
