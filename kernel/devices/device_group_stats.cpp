//
// Created by sigsegv on 3/11/22.
//

#include <stats/statistics_visitor.h>
#include "device_group_stats.h"

void device_stats::Accept(statistics_visitor &visitor) {
    visitor.Visit("device", *DeviceStats);
}

void device_group_stats::Accept(statistics_visitor &visitor) {
    for (auto &device : devices) {
        device_stats sub{std::get<1>(device)};
        visitor.Visit(std::get<0>(device), sub);
    }
}