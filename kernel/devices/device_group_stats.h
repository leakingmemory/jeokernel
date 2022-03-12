//
// Created by sigsegv on 3/11/22.
//

#ifndef JEOKERNEL_DEVICE_GROUP_STATS_H
#define JEOKERNEL_DEVICE_GROUP_STATS_H

#include <stats/statistics_object.h>
#include <string>
#include <tuple>
#include <vector>

class device_stats : public statistics_object {
private:
    std::shared_ptr<statistics_object> DeviceStats;
public:
    device_stats(std::shared_ptr<statistics_object> DeviceStats) : DeviceStats(DeviceStats) {}
    void Accept(statistics_visitor &visitor) override;
};

class device_group_stats : public statistics_object {
private:
    std::vector<std::tuple<std::string,std::shared_ptr<statistics_object>>> devices;
public:
    device_group_stats(const std::vector<std::tuple<std::string,std::shared_ptr<statistics_object>>> &devices) : devices(devices) {}
    void Accept(statistics_visitor &visitor) override;
};


#endif //JEOKERNEL_DEVICE_GROUP_STATS_H
