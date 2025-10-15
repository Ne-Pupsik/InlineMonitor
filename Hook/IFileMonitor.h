#ifndef IFILEMONITOR_H
#define IFILEMONITOR_H

#include "MonitorClient.h"

class IFileMonitor {
public:
    virtual ~IFileMonitor() = default;

    virtual void configure(const Config& config, MonitorClient* client) = 0;

    virtual bool installHooks() = 0;

    virtual bool uninstallHooks() = 0;
};

#endif // IFILEMONITOR_H