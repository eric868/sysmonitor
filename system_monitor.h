/******************************************************************************
Copyright (c) 2016. All Rights Reserved.

FileName: system_monitor.h
Version: 1.0
Date: 2018.3.17

History:
eric     2018.3.17   1.0     Create
******************************************************************************/

#ifndef SYSTEM_MONITOR_H_
#define SYSTEM_MONITOR_H_

#include <stdint.h>
#include <string>

namespace base
{
#define IN
#define OUT

struct MemInfo
{
    int total;
    int free;
    int buffers;
    int cached;
};

class SystemMonitor
{
public:
    SystemMonitor();

public:
    static std::string GetSysMac();
    static int GetprocessCount(IN const char * process_name);
    static bool IsProcessRunning(IN const char *process_name);

    static int GetMemInfo(OUT MemInfo& mi);
    static int GetProcessMem(const pid_t pid);
    static int GetTotalMem();
    static double GetProcessMemPercent(pid_t pid);

    static double GetTotalCpuPercent();
    static double GetProcessCpuPercent(pid_t pid);

    static void GetProcessCpuInfo(IN const char *process_name, OUT unsigned int *proc_nums,
            OUT unsigned int *res);

    static void GetProcessMemInfo(IN const char *process_name, OUT unsigned int *proc_nums,
            OUT unsigned int *res);

    //check disk stat of the path ,KB,1-100%
    static void GetDiskPercent(IN const char *path, OUT int *free,
            OUT int *total, OUT double *Usage);

private:
    static bool IsNumeric(const char* str);
    static const char* GetItems(const char* buffer, int ie);
    static unsigned int GetProcessCpuOccupy(const pid_t pid);
    static unsigned int GetTotalCpuOccupy();
    static int GetTotalCpuOccupy(void* param);
};
}
#endif
