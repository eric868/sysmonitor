/*****************************************************************************
VBase Copyright (c) 2015. All Rights Reserved.

FileName: main.cpp
Version: 1.0
Date: 2016.11.25

History:
eric     2016.11.25   1.0     Create
******************************************************************************/
#include <map>
#include <string>
#include <iostream>
#include <stdio.h>

#include "system_monitor.h"

using namespace std;

int main(int argc, char* argv[])
{
    SystemMonitor inst;
    unsigned int cpu_usage = 0;
    inst.GetCpuUsage(&cpu_usage);
    cout << "cpu_usage: " << cpu_usage << endl;
    int used, free, total;
    inst.GetMemUsage(&used, &free, &total);
    cout << "used: " << used << " free: " << free << " total: " << total << endl;

    const char* path ="/";
    int  usage;
    inst.GetDiskUsage(path, &free, &total, &usage);
    cout << "used: " << usage << " free: " << free << " total: " << total << endl;

    std::string mac;
    inst.GetSysMac(mac);
    cout << "mac: " << mac << endl;

    int count = inst.GetprocessCount("top");
    cout << "top: " << count << endl;
    return 0;
}
