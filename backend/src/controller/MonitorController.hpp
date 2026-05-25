#ifndef MonitorController_hpp
#define MonitorController_hpp

#include "dto/MonitorDto.hpp"
#include "service/MonitorService.hpp"

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"
#include <mutex>

#include OATPP_CODEGEN_BEGIN(ApiController)

class MonitorController : public oatpp::web::server::api::ApiController {
private:
    std::shared_ptr<MonitorService> m_monitorService;
    oatpp::Object<ConfigDto> m_config;
    std::mutex m_configMutex;

public:
    MonitorController(OATPP_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, apiObjectMapper))
        : oatpp::web::server::api::ApiController(apiObjectMapper)
    {
        m_monitorService = std::make_shared<MonitorService>();
        
        // Initialize default configuration
        m_config = ConfigDto::createShared();
        m_config->pollingIntervalMs = 1000;
        m_config->enableCpu = true;
        m_config->enableMemory = true;
        m_config->enableDisk = true;
        m_config->enableNetwork = true;
        m_config->enableProcesses = true;
        m_config->maxProcesses = 10;
    }

    static std::shared_ptr<MonitorController> createShared(
        OATPP_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, apiObjectMapper)
    ) {
        return std::make_shared<MonitorController>(apiObjectMapper);
    }

    ENDPOINT_INFO(getMetrics) {
        info->summary = "Get current macOS system metrics";
        info->description = "Returns real-time system metrics based on current configuration";
        info->addResponse<Object<MetricsResponseDto>>(Status::CODE_200, "application/json");
    }
    ENDPOINT("GET", "/api/metrics", getMetrics) {
        oatpp::Object<ConfigDto> currentConfig;
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            currentConfig = m_config;
        }
        return createDtoResponse(Status::CODE_200, m_monitorService->getMetrics(currentConfig));
    }

    ENDPOINT_INFO(getConfig) {
        info->summary = "Get current telemetry configuration";
        info->addResponse<Object<ConfigDto>>(Status::CODE_200, "application/json");
    }
    ENDPOINT("GET", "/api/config", getConfig) {
        std::lock_guard<std::mutex> lock(m_configMutex);
        return createDtoResponse(Status::CODE_200, m_config);
    }

    ENDPOINT_INFO(updateConfig) {
        info->summary = "Update telemetry configuration";
        info->addConsumes<Object<ConfigDto>>("application/json");
        info->addResponse<Object<ConfigDto>>(Status::CODE_200, "application/json");
    }
    ENDPOINT("POST", "/api/config", updateConfig, BODY_DTO(Object<ConfigDto>, configDto)) {
        std::lock_guard<std::mutex> lock(m_configMutex);
        // Make sure fields are populated safely
        if (!configDto->pollingIntervalMs) configDto->pollingIntervalMs = 1000;
        if (configDto->maxProcesses == nullptr) configDto->maxProcesses = 10;
        
        m_config = configDto;
        return createDtoResponse(Status::CODE_200, m_config);
    }
};

#include OATPP_CODEGEN_END(ApiController)

#endif // MonitorController_hpp
