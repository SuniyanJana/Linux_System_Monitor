#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <string>
#include "../common/SystemData.h"

class SystemMonitor {
public:
    std::string getHostname();
    double getMemoryUsage();
    double getDiskUsage();
    int getProcessCount();
    double getUptime();
    std::string getKernelVersion();
    double getCpuUsage();
    SystemData collect();
};

#endif
