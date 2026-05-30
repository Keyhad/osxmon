#ifndef MonitorDto_hpp
#define MonitorDto_hpp

#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/Types.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

class CpuCoreDto : public oatpp::DTO {
  DTO_INIT(CpuCoreDto, DTO)
  DTO_FIELD(Int32,   id);
  DTO_FIELD(Float64, user);
  DTO_FIELD(Float64, system);
  DTO_FIELD(Float64, idle);
  DTO_FIELD(Float64, load); // user + system
};

class CpuMetricsDto : public oatpp::DTO {
  DTO_INIT(CpuMetricsDto, DTO)
  DTO_FIELD(Float64, system);
  DTO_FIELD(Float64, user);
  DTO_FIELD(Float64, idle);
  DTO_FIELD(Float64, temperature);             // Celsius, -1 if unavailable
  DTO_FIELD(List<Object<CpuCoreDto>>, cores);  // per-logical-core loads
};

class MemoryMetricsDto : public oatpp::DTO {
  DTO_INIT(MemoryMetricsDto, DTO)
  DTO_FIELD(Int64, total);
  DTO_FIELD(Int64, free);
  DTO_FIELD(Int64, used);
  DTO_FIELD(Int64, active);
  DTO_FIELD(Int64, inactive);
  DTO_FIELD(Int64, wired);
  DTO_FIELD(Int64, compressed);
};

class DiskMetricsDto : public oatpp::DTO {
  DTO_INIT(DiskMetricsDto, DTO)
  DTO_FIELD(String, mountPoint);
  DTO_FIELD(Int64, total);
  DTO_FIELD(Int64, used);
  DTO_FIELD(Int64, free);
  DTO_FIELD(Float64, usedPercent);
};

class NetworkMetricsDto : public oatpp::DTO {
  DTO_INIT(NetworkMetricsDto, DTO)
  DTO_FIELD(String, interface);
  DTO_FIELD(Int64, inputBytes);
  DTO_FIELD(Int64, outputBytes);
  DTO_FIELD(Float64, inputSpeed); // Bytes/sec
  DTO_FIELD(Float64, outputSpeed); // Bytes/sec
};

class ProcessMetricsDto : public oatpp::DTO {
  DTO_INIT(ProcessMetricsDto, DTO)
  DTO_FIELD(Int32, pid);
  DTO_FIELD(String, name);
  DTO_FIELD(Float64, cpuPercent);
  DTO_FIELD(Int64, residentMemory); // Bytes
};

class ConfigDto : public oatpp::DTO {
  DTO_INIT(ConfigDto, DTO)
  DTO_FIELD(String, title);
  DTO_FIELD(Int32, pollingIntervalMs);
  DTO_FIELD(Boolean, enableCpu);
  DTO_FIELD(Boolean, enableMemory);
  DTO_FIELD(Boolean, enableDisk);
  DTO_FIELD(Boolean, enableNetwork);
  DTO_FIELD(Boolean, enableProcesses);
  DTO_FIELD(Int32, maxProcesses);
  DTO_FIELD(List<String>, monitoredProcesses);
};

class MetricsResponseDto : public oatpp::DTO {
  DTO_INIT(MetricsResponseDto, DTO)
  DTO_FIELD(Object<CpuMetricsDto>, cpu);
  DTO_FIELD(Object<MemoryMetricsDto>, memory);
  DTO_FIELD(List<Object<DiskMetricsDto>>, disks);
  DTO_FIELD(List<Object<NetworkMetricsDto>>, network);
  DTO_FIELD(List<Object<ProcessMetricsDto>>, processes);
  DTO_FIELD(Object<ConfigDto>, config);
  DTO_FIELD(Int64, timestamp); // Milliseconds since epoch
};

#include OATPP_CODEGEN_END(DTO)

#endif // MonitorDto_hpp
