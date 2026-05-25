#ifndef MonitorService_hpp
#define MonitorService_hpp

#include <iostream>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <cmath>
#include <algorithm>

#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/vm_map.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <libproc.h>
#include <net/if.h>
#include <net/route.h>
#include <unistd.h>

#include "dto/MonitorDto.hpp"
#include "oatpp/core/Types.hpp"

class MonitorService {
private:
    std::mutex m_mutex;
    
    // CPU Ticks cache
    host_cpu_load_info_data_t m_prevCpuLoad;
    bool m_hasPrevCpu = false;

    // Network stats cache
    struct NetCounter {
        uint64_t ibytes = 0;
        uint64_t obytes = 0;
    };
    std::unordered_map<std::string, NetCounter> m_prevNetStats;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_prevNetTimes;

    // Process stats cache
    std::unordered_map<pid_t, uint64_t> m_prevProcTimes; // pid -> total cpu time in ns
    std::unordered_map<pid_t, std::chrono::steady_clock::time_point> m_prevProcTimesNow;

    int m_cpuCores = 1;

    int getCpuCores() {
        int cores = 1;
        size_t size = sizeof(cores);
        if (sysctlbyname("hw.ncpu", &cores, &size, nullptr, 0) != 0) {
            cores = 1;
        }
        return cores;
    }

public:
    MonitorService() {
        m_cpuCores = getCpuCores();
        // Zero initialize CPU stats
        m_prevCpuLoad.cpu_ticks[CPU_STATE_USER] = 0;
        m_prevCpuLoad.cpu_ticks[CPU_STATE_SYSTEM] = 0;
        m_prevCpuLoad.cpu_ticks[CPU_STATE_IDLE] = 0;
        m_prevCpuLoad.cpu_ticks[CPU_STATE_NICE] = 0;
    }

    oatpp::Object<MetricsResponseDto> getMetrics(const oatpp::Object<ConfigDto>& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto response = MetricsResponseDto::createShared();
        auto now = std::chrono::steady_clock::now();
        auto systemNow = std::chrono::system_clock::now();
        response->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            systemNow.time_since_epoch()
        ).count();
        response->config = config;

        // 1. CPU Metrics
        if (config->enableCpu) {
            host_cpu_load_info_data_t currCpuLoad;
            mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
            if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&currCpuLoad, &count) == KERN_SUCCESS) {
                if (m_hasPrevCpu) {
                    uint64_t userDiff = currCpuLoad.cpu_ticks[CPU_STATE_USER] - m_prevCpuLoad.cpu_ticks[CPU_STATE_USER];
                    uint64_t systemDiff = currCpuLoad.cpu_ticks[CPU_STATE_SYSTEM] - m_prevCpuLoad.cpu_ticks[CPU_STATE_SYSTEM];
                    uint64_t idleDiff = currCpuLoad.cpu_ticks[CPU_STATE_IDLE] - m_prevCpuLoad.cpu_ticks[CPU_STATE_IDLE];
                    uint64_t niceDiff = currCpuLoad.cpu_ticks[CPU_STATE_NICE] - m_prevCpuLoad.cpu_ticks[CPU_STATE_NICE];
                    
                    uint64_t totalDiff = userDiff + systemDiff + idleDiff + niceDiff;
                    if (totalDiff > 0) {
                        auto cpuDto = CpuMetricsDto::createShared();
                        cpuDto->user = (double)userDiff / totalDiff * 100.0;
                        cpuDto->system = (double)systemDiff / totalDiff * 100.0;
                        cpuDto->idle = (double)idleDiff / totalDiff * 100.0;
                        response->cpu = cpuDto;
                    }
                } else {
                    auto cpuDto = CpuMetricsDto::createShared();
                    cpuDto->user = 0.0;
                    cpuDto->system = 0.0;
                    cpuDto->idle = 100.0;
                    response->cpu = cpuDto;
                    m_hasPrevCpu = true;
                }
                m_prevCpuLoad = currCpuLoad;
            }
        }

        // 2. Memory Metrics
        if (config->enableMemory) {
            int64_t totalMem = 0;
            size_t len = sizeof(totalMem);
            if (sysctlbyname("hw.memsize", &totalMem, &len, nullptr, 0) == 0) {
                mach_port_t host_port = mach_host_self();
                vm_size_t page_size;
                if (host_page_size(host_port, &page_size) == KERN_SUCCESS) {
                    vm_statistics64_data_t vm_stats;
                    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
                    if (host_statistics64(host_port, HOST_VM_INFO64, (host_info64_t)&vm_stats, &count) == KERN_SUCCESS) {
                        auto memDto = MemoryMetricsDto::createShared();
                        memDto->total = totalMem;
                        memDto->free = (int64_t)vm_stats.free_count * page_size;
                        memDto->active = (int64_t)vm_stats.active_count * page_size;
                        memDto->inactive = (int64_t)vm_stats.inactive_count * page_size;
                        memDto->wired = (int64_t)vm_stats.wire_count * page_size;
                        memDto->compressed = (int64_t)vm_stats.compressor_page_count * page_size;
                        
                        // Used Memory: Active + Wired + Compressed
                        memDto->used = memDto->active + memDto->wired + memDto->compressed;
                        
                        response->memory = memDto;
                    }
                }
            }
        }

        // 3. Disk Metrics
        if (config->enableDisk) {
            response->disks = oatpp::List<oatpp::Object<DiskMetricsDto>>::createShared();
            int num_fs = getfsstat(nullptr, 0, MNT_NOWAIT);
            if (num_fs > 0) {
                std::vector<struct statfs> fs_list(num_fs);
                num_fs = getfsstat(fs_list.data(), num_fs * sizeof(struct statfs), MNT_NOWAIT);
                for (int i = 0; i < num_fs; ++i) {
                    std::string mount_point = fs_list[i].f_mntonname;
                    // Only track root mount or volumes that are mounted under /Volumes/
                    if (mount_point == "/" || mount_point.rfind("/Volumes/", 0) == 0) {
                        auto diskDto = DiskMetricsDto::createShared();
                        diskDto->mountPoint = mount_point.c_str();
                        diskDto->total = (int64_t)fs_list[i].f_blocks * fs_list[i].f_bsize;
                        diskDto->free = (int64_t)fs_list[i].f_bavail * fs_list[i].f_bsize;
                        diskDto->used = diskDto->total - diskDto->free;
                        diskDto->usedPercent = diskDto->total > 0 ? ((double)diskDto->used / diskDto->total) * 100.0 : 0.0;
                        response->disks->push_back(diskDto);
                    }
                }
            }
        }

        // 4. Network Metrics
        if (config->enableNetwork) {
            response->network = oatpp::List<oatpp::Object<NetworkMetricsDto>>::createShared();
            int mib[] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0};
            size_t len;
            if (sysctl(mib, 6, nullptr, &len, nullptr, 0) == 0) {
                std::vector<char> buf(len);
                if (sysctl(mib, 6, buf.data(), &len, nullptr, 0) == 0) {
                    char *lim = buf.data() + len;
                    for (char *next = buf.data(); next < lim; ) {
                        struct if_msghdr *ifm = (struct if_msghdr *)next;
                        next += ifm->ifm_msglen;

                        if (ifm->ifm_type == RTM_IFINFO2) {
                            struct if_msghdr2 *if2m = (struct if_msghdr2 *)ifm;
                            char name_buf[IF_NAMESIZE];
                            if (if_indextoname(if2m->ifm_index, name_buf) != nullptr) {
                                std::string ifname(name_buf);
                                
                                // Ignore loopback interface
                                if (ifname == "lo0") continue;

                                uint64_t cur_ibytes = if2m->ifm_data.ifi_ibytes;
                                uint64_t cur_obytes = if2m->ifm_data.ifi_obytes;

                                // If both are 0, skip interface
                                if (cur_ibytes == 0 && cur_obytes == 0) continue;

                                double input_speed = 0.0;
                                double output_speed = 0.0;

                                if (m_prevNetTimes.find(ifname) != m_prevNetTimes.end()) {
                                    double elapsed_sec = std::chrono::duration<double>(now - m_prevNetTimes[ifname]).count();
                                    if (elapsed_sec > 0.05) {
                                        uint64_t idelta = (cur_ibytes >= m_prevNetStats[ifname].ibytes) ? 
                                            (cur_ibytes - m_prevNetStats[ifname].ibytes) : 0;
                                        uint64_t odelta = (cur_obytes >= m_prevNetStats[ifname].obytes) ? 
                                            (cur_obytes - m_prevNetStats[ifname].obytes) : 0;

                                        input_speed = (double)idelta / elapsed_sec;
                                        output_speed = (double)odelta / elapsed_sec;
                                    }
                                }

                                m_prevNetStats[ifname] = {cur_ibytes, cur_obytes};
                                m_prevNetTimes[ifname] = now;

                                auto netDto = NetworkMetricsDto::createShared();
                                netDto->interface = ifname.c_str();
                                netDto->inputBytes = cur_ibytes;
                                netDto->outputBytes = cur_obytes;
                                netDto->inputSpeed = input_speed;
                                netDto->outputSpeed = output_speed;
                                response->network->push_back(netDto);
                            }
                        }
                    }
                }
            }
        }

        // 5. Process Metrics
        if (config->enableProcesses) {
            response->processes = oatpp::List<oatpp::Object<ProcessMetricsDto>>::createShared();
            int num_pids = proc_listallpids(nullptr, 0);
            if (num_pids > 0) {
                std::vector<pid_t> pids(num_pids);
                num_pids = proc_listallpids(pids.data(), num_pids * sizeof(pid_t));
                
                std::unordered_map<pid_t, uint64_t> nextProcTimes;
                std::unordered_map<pid_t, std::chrono::steady_clock::time_point> nextProcTimesNow;
                std::vector<oatpp::Object<ProcessMetricsDto>> procList;

                for (int i = 0; i < num_pids; ++i) {
                    pid_t pid = pids[i];
                    if (pid <= 0) continue;

                    struct proc_taskinfo taskInfo;
                    int ret = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &taskInfo, sizeof(taskInfo));
                    if (ret == sizeof(taskInfo)) {
                        char name_buf[256];
                        proc_name(pid, name_buf, sizeof(name_buf));
                        std::string pname(name_buf);
                        if (pname.empty()) pname = "Unknown";

                        // CPU nanoseconds (user + system)
                        uint64_t cur_proc_time = taskInfo.pti_total_user + taskInfo.pti_total_system;
                        nextProcTimes[pid] = cur_proc_time;
                        nextProcTimesNow[pid] = now;

                        double cpuPercent = 0.0;
                        if (m_prevProcTimes.find(pid) != m_prevProcTimes.end()) {
                            uint64_t delta_proc_time = cur_proc_time - m_prevProcTimes[pid];
                            double delta_real_sec = std::chrono::duration<double>(now - m_prevProcTimesNow[pid]).count();
                            if (delta_real_sec > 0.05) {
                                double delta_real_ns = delta_real_sec * 1e9;
                                // Multiply by 100 for percentage
                                cpuPercent = (delta_real_ns > 0) ? ((double)delta_proc_time / delta_real_ns) * 100.0 : 0.0;
                                // Divide by cores so that total system CPU is bounded at 100%
                                cpuPercent /= m_cpuCores;
                            }
                        }

                        auto procDto = ProcessMetricsDto::createShared();
                        procDto->pid = pid;
                        procDto->name = pname.c_str();
                        procDto->cpuPercent = std::round(cpuPercent * 10.0) / 10.0; // Round to 1 decimal place
                        procDto->residentMemory = taskInfo.pti_resident_size;
                        procList.push_back(procDto);
                    }
                }

                // Swap previous caches to clean up dead PIDs
                m_prevProcTimes = std::move(nextProcTimes);
                m_prevProcTimesNow = std::move(nextProcTimesNow);

                // Sort processes by CPU usage descending, then by Memory
                std::sort(procList.begin(), procList.end(), [](const oatpp::Object<ProcessMetricsDto>& a, const oatpp::Object<ProcessMetricsDto>& b) {
                    if (a->cpuPercent != b->cpuPercent) {
                        return a->cpuPercent > b->cpuPercent;
                    }
                    return a->residentMemory > b->residentMemory;
                });

                // Take top processes up to config->maxProcesses
                int count = 0;
                int maxP = (config->maxProcesses && *config->maxProcesses > 0) ? *config->maxProcesses : 10;
                for (const auto& proc : procList) {
                    if (count >= maxP) break;
                    response->processes->push_back(proc);
                    count++;
                }
            }
        }

        return response;
    }
};

#endif // MonitorService_hpp
