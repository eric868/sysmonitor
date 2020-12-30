/******************************************************************************
Copyright (c) 2016. All Rights Reserved.

FileName: system_monitor.cpp
Version: 1.0
Date: 2016.1.13

History:
eric     2016.1.13   1.0     Create
******************************************************************************/

#include "system_monitor.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <dirent.h>
#include <assert.h>

#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>
#include <fstream>
namespace base
{
#define SYS_MAC "ens33"
#define PROC_STAT_PATH "/proc/stat"
#define PROC_MEMINFO_PATH "/proc/meminfo"
#define PROC_DIRECTORY "/proc/"

#define VMRSS_LINE 17       /* VMRSS所在行 */
#define PROCESS_ITEM 14     /* 进程CPU时间开始的项数 */

typedef struct
{
    unsigned int user;      /* 从系统启动开始累计到当前时刻，处于用户态的运行时间，不包含 nice值为负进程。 */
    unsigned int nice;      /* 从系统启动开始累计到当前时刻，nice值为负的进程所占用的CPU时间 */
    unsigned int system;    /* 从系统启动开始累计到当前时刻，处于核心态的运行时间 */
    unsigned int idle;      /* 从系统启动开始累计到当前时刻，除IO等待时间以外的其它等待时间iowait (12256) 从系统启动开始累计到当前时刻，IO等待时间(since 2.5.41) */
    unsigned int iowait;
} total_cpu_occupy_t;

typedef struct
{
    pid_t pid;              /* pid号 */
    unsigned int utime;     /*该任务在用户态运行的时间，单位为jiffies */
    unsigned int stime;     /* 该任务在核心态运行的时间，单位为jiffies */
    unsigned int cutime;    /* 所有已死线程在用户态运行的时间，单位为jiffies */
    unsigned int cstime;    /* 所有已死在核心态运行的时间，单位为jiffies */
    unsigned int iowait;
} process_cpu_occupy_t;

SystemMonitor::SystemMonitor()
{
}

std::string SystemMonitor::GetSysMac()
{
    int sockfd;
    struct ifreq tmp;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd<0) {
        printf("GetSysMac::create socket fail\n");
        return "";
    }

    memset(&tmp, 0, sizeof(struct ifreq));
    strncpy(tmp.ifr_name, SYS_MAC, sizeof(tmp.ifr_name) - 1);
    if ((ioctl(sockfd, SIOCGIFHWADDR, &tmp))<0) {
        printf("GetSysMac::mac ioctl error \n");
        close(sockfd);
        return "";
    }
    close(sockfd);

    char data[64];
    sprintf(data, "%02X-%02X-%02X-%02X-%02X-%02X",
        (unsigned char)tmp.ifr_hwaddr.sa_data[0],
        (unsigned char)tmp.ifr_hwaddr.sa_data[1],
        (unsigned char)tmp.ifr_hwaddr.sa_data[2],
        (unsigned char)tmp.ifr_hwaddr.sa_data[3],
        (unsigned char)tmp.ifr_hwaddr.sa_data[4],
        (unsigned char)tmp.ifr_hwaddr.sa_data[5]);

    return data;
}

int SystemMonitor::GetprocessCount(const char * process_name)
{
    char status_path[100] ={0} ;
    char proc_name_line[300] ={0} ;
    char proc_name[64]={0};

    pid_t proc_count = 0;

    DIR* dir_proc = NULL ;
    dir_proc = opendir(PROC_DIRECTORY) ;
    if (dir_proc == NULL) {
        perror("Couldn't open the " PROC_DIRECTORY " directory") ;
        return -1 ;
    }
    struct dirent* de_DirEntity = NULL ;
    // Loop while not NULL
    while ( (de_DirEntity = readdir(dir_proc)) ) {
        if (de_DirEntity->d_type == DT_DIR) {
            if (IsNumeric(de_DirEntity->d_name)) {
                strcpy(status_path, PROC_DIRECTORY) ;
                strcat(status_path, de_DirEntity->d_name) ;
                strcat(status_path, "/status") ;
                FILE* fd = fopen (status_path, "rt");
                if (fd) {
                    fgets(proc_name_line,sizeof(proc_name_line),fd);
                    sscanf(proc_name_line,"%*s%s",proc_name);
                    fclose(fd);
                    if (!strcmp(proc_name,process_name)) {
                        proc_count++;
                    }
                }
                else {
                    perror(" fopen status_path failed");
                }
            }
        }
    }
    closedir(dir_proc) ;
    return proc_count ;
}

bool SystemMonitor::IsProcessRunning(const char *process_name)
{
    return (GetprocessCount(process_name) > 0) ? true : false;
}

int SystemMonitor::GetMemInfo(MemInfo& mi)
{
    FILE* fd = fopen(PROC_MEMINFO_PATH, "r");
    if (fd == NULL) {
        return -1;
    }

    char line[256] = {0};
    char name[32];

    fgets(line, sizeof(line), fd);
    sscanf(line, "%s %d", name, &mi.total);

    fgets(line, sizeof(line), fd);
    sscanf(line, "%s %d", name, &mi.free);

    fgets(line, sizeof(line), fd);//MemAvailable
    fgets(line, sizeof(line), fd);
    sscanf(line, "%s %d", name, &mi.buffers);

    fgets(line, sizeof(line), fd);
    sscanf(line, "%s %d", name, &mi.cached);

    fclose(fd);
    return 0;
}

int SystemMonitor::GetProcessMem(const pid_t pid)
{
    char file[64] = {0};
    char line[256] = {0};

    sprintf(file, "/proc/%d/status", pid);
    FILE* fd = fopen(file, "r");
    if (fd == NULL) {
        return -1;
    }

    for (int i = 0; i < VMRSS_LINE; i++) {
        fgets(line, sizeof(line), fd);
    }

    int vmrss;
    char name[32];
    sscanf(line, "%s %d", name, &vmrss);

    fclose(fd);
    return vmrss;
}

int SystemMonitor::GetTotalMem()
{
    FILE* fd = fopen("/proc/meminfo", "r");
    if (fd == NULL) {
        return -1;
    }

    char line[256] = {0};
    fgets(line, sizeof(line), fd);

    char name[32];
    int memtotal = 0;
    sscanf(line, "%s %d", name, &memtotal);

    fclose(fd);
    return memtotal == 0 ? 1 : memtotal;
}

double SystemMonitor::GetProcessMemPercent(pid_t pid)
{
    int pid_mem = GetProcessMem(pid) * 100;
    int total_mem = GetTotalMem();

    return pid_mem * 1.0 / total_mem;
}

double SystemMonitor::GetTotalCpuPercent()
{
    total_cpu_occupy_t t1;
    GetTotalCpuOccupy(&t1);

    usleep(200000);

    total_cpu_occupy_t t2;
    GetTotalCpuOccupy(&t2);

    double diff = (t2.user + t2.nice + t2.system + t2.idle + t2.iowait);
    diff -= (t1.user + t1.nice + t1.system + t1.idle + t1.iowait);
    diff = (diff == 0 ? 1 : diff);

    double usage = 1.0 - (1.0 * (t2.idle - t1.idle) / diff);
    return usage;
}

double SystemMonitor::GetProcessCpuPercent(pid_t pid)
{
    unsigned int proc_cpu_time1, proc_cpu_time2;
    unsigned int total_cpu_time1, total_cpu_time2;

    proc_cpu_time1 = GetProcessCpuOccupy(pid);
    total_cpu_time1 = GetTotalCpuOccupy();

    usleep(500000);

    proc_cpu_time2 = GetProcessCpuOccupy(pid);
    total_cpu_time2 = GetTotalCpuOccupy();

    double cpu_percent = 1.0 * (proc_cpu_time2 - proc_cpu_time1) / (total_cpu_time2 - total_cpu_time1);
    return cpu_percent;
}

void SystemMonitor::GetProcessCpuInfo(IN const char *process_name, OUT unsigned int *proc_nums,
        OUT unsigned int *res){
    *res = 0;
    *proc_nums = 0;

    char status_path[100] ="\0";
    char proc_name_line[300] = "\0";
    char proc_name[64]= "\0";
    char proc_pid_flag[100] = "\0";
    unsigned int proc_pid =0;

    DIR *dir_proc = NULL;
    dir_proc = opendir(PROC_DIRECTORY) ;
    if (dir_proc == NULL) {
        perror("Couldn't open the " PROC_DIRECTORY " directory") ;
        return ;
    }
    struct dirent* de_DirEntity = NULL ;
    while ( (de_DirEntity = readdir(dir_proc)) ) {
        if (de_DirEntity->d_type == DT_DIR) {
            if (IsNumeric(de_DirEntity->d_name)){
                strcpy(status_path, PROC_DIRECTORY) ;
                strcat(status_path, de_DirEntity->d_name) ;
                strcat(status_path, "/status") ;
                FILE * fp = fopen(status_path,"rt");
                if (fp) {
                    fgets(proc_name_line,sizeof(proc_name_line),fp);
                    sscanf(proc_name_line,"%*s%s",proc_name);
                    if (!strcmp(proc_name,process_name)) {
                        fgets(proc_pid_flag,sizeof(proc_pid_flag),fp);
                        fgets(proc_pid_flag,sizeof(proc_pid_flag),fp);
                        sscanf(proc_pid_flag,"%*s%d",&proc_pid);
                        (*proc_nums)++;
                        (*res) += GetProcessCpuPercent(proc_pid);
                    }
                    fclose(fp);
                }
            }
        }
    }
    closedir(dir_proc);
}

void SystemMonitor::GetProcessMemInfo(IN const char *process_name, OUT unsigned int *proc_nums,
        OUT unsigned int *res)
{
    *res = 0;
    *proc_nums =0;

    char status_path[100] ="\0";
    char proc_name_line[100] = "\0";
    char proc_name[64]= "\0";
    char proc_pid_flag[100] = "\0";
    unsigned int proc_pid =0;

    DIR *dir_proc = NULL;
    dir_proc = opendir(PROC_DIRECTORY);
    if (dir_proc == NULL) {
        perror("Couldn't open the " PROC_DIRECTORY  " directory") ;
        return ;
    }
    struct dirent* de_DirEntity = NULL;
    while ( (de_DirEntity = readdir(dir_proc)) ) {
        if (de_DirEntity->d_type == DT_DIR) {
            if (IsNumeric(de_DirEntity->d_name)){
                strcpy(status_path, PROC_DIRECTORY) ;
                strcat(status_path, de_DirEntity->d_name) ;
                strcat(status_path, "/status") ;
                FILE * fp = fopen(status_path,"rt");
                if (fp) {
                    fgets(proc_name_line,sizeof(proc_name_line),fp);
                    sscanf(proc_name_line,"%*s%s",proc_name);
                    if (!strcmp(proc_name,process_name)) {
                        fgets(proc_pid_flag,sizeof(proc_pid_flag),fp);
                        fgets(proc_pid_flag,sizeof(proc_pid_flag),fp);
                        sscanf(proc_pid_flag,"%*s%d",&proc_pid);
                        (*proc_nums)++;
                        (*res) += GetProcessMemPercent(proc_pid);
                    }
                    fclose(fp);
                }
            }
        }
    }
    closedir(dir_proc);
}

void SystemMonitor::GetDiskPercent(IN const char *path, OUT int *free,
        OUT int *total, OUT double *Usage)
{
    struct statfs path_disk;
    if(statfs(path,&path_disk)< 0 ) {
        perror("path disk can't");
        return ;
    }
    (*free) = (long long )path_disk.f_bsize * (long long )path_disk.f_bfree/1024;
    (*total) = (long long )path_disk.f_bsize * (long long )path_disk.f_blocks/1024;
    (*Usage) = ((*total)-(*free)) * 100 * 1.0 / (*total);
}


bool SystemMonitor::IsNumeric(const char* str)
{
    for ( ; *str; ++str)
        if (*str < '0' || *str > '9')
            return false;
    return true;
}

const char* SystemMonitor::GetItems(const char* buffer, int ie)
{
    assert(buffer);
    const char* p = buffer;
    int len = strlen(buffer);
    int count = 0;
    if (1 == ie || ie < 1) {
        return p;
    }
    for (int i=0; i<len; i++) {
        if (' ' == *p) {
            count++;
            if (count == ie-1) {
                p++;
                break;
            }
        }
        p++;
    }

    return p;
}

unsigned int SystemMonitor::GetProcessCpuOccupy(const pid_t pid)
{
    char file[64] = {0};
    char line[1024] = {0};

    sprintf(file, "/proc/%d/stat", pid);
    FILE* fd = fopen(file, "r");
    if (fd == NULL) {
        return 0;
    }
    fgets(line, sizeof(line), fd);

    process_cpu_occupy_t t;
    sscanf(line, "%u", &t.pid);

    const char* q = GetItems((const char*)line, PROCESS_ITEM);
    sscanf(q, "%u %u %u %u %u", &t.utime, &t.stime, &t.cutime, &t.cstime, &t.iowait);

    fclose(fd);
    return (t.utime + t.stime + t.cutime + t.cstime);
}


unsigned int SystemMonitor::GetTotalCpuOccupy()
{
    FILE* fd = fopen(PROC_STAT_PATH, "r");
    if (fd == NULL) {
        return -1;
    }

    char line[1024] = {0};
    fgets(line, sizeof(line), fd);

    char name[32];
    total_cpu_occupy_t t;
    sscanf(line, "%s %u %u %u %u %u", name, &t.user,
            &t.nice, &t.system, &t.idle, &t.iowait);

    fclose(fd);
    return (t.user + t.nice + t.system + t.iowait + t.idle);
}

int SystemMonitor::GetTotalCpuOccupy(void* param)
{
    total_cpu_occupy_t* t = (total_cpu_occupy_t*)param;

    FILE* fd = fopen(PROC_STAT_PATH, "r");
    if (fd == NULL) {
        return -1;
    }

    char line[1024] = {0};
    fgets(line, sizeof(line), fd);

    char name[32];
    sscanf(line, "%s %u %u %u %u %u", name, &t->user,
            &t->nice, &t->system, &t->idle, &t->iowait);

    fclose(fd);
    return 0;
}

}
