//
// Created by sigsegv on 9/11/21.
//

#include <stats/statistics_visitor.h>
#include "StructPool.h"

void StructPoolStats::Accept(statistics_visitor &visitor) {
    visitor.Visit("allocators", numAllocators);
}
