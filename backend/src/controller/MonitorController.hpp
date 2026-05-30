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

    static oatpp::Object<ConfigDto> createDefaultConfig() {
        auto config = ConfigDto::createShared();
        config->title = "osxmon dashboard";
        config->pollingIntervalMs = 1000;
        config->enableCpu = true;
        config->enableMemory = true;
        config->enableDisk = true;
        config->enableNetwork = true;
        config->enableProcesses = true;
        config->maxProcesses = 10;
        config->monitoredProcesses = oatpp::List<oatpp::String>::createShared();
        return config;
    }

    static void applyDefaults(const oatpp::Object<ConfigDto>& config) {
        if (!config) return;
        if (!config->title) config->title = "osxmon dashboard";
        if (!config->pollingIntervalMs) config->pollingIntervalMs = 1000;
        if (!config->enableCpu) config->enableCpu = true;
        if (!config->enableMemory) config->enableMemory = true;
        if (!config->enableDisk) config->enableDisk = true;
        if (!config->enableNetwork) config->enableNetwork = true;
        if (!config->enableProcesses) config->enableProcesses = true;
        if (!config->maxProcesses) config->maxProcesses = 10;
        if (!config->monitoredProcesses) config->monitoredProcesses = oatpp::List<oatpp::String>::createShared();
    }

public:
    MonitorController(
        OATPP_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, apiObjectMapper),
        const oatpp::Object<ConfigDto>& initialConfig = nullptr
    )
        : oatpp::web::server::api::ApiController(apiObjectMapper)
    {
        m_monitorService = std::make_shared<MonitorService>();

        m_config = initialConfig ? initialConfig : createDefaultConfig();
        applyDefaults(m_config);
    }

    static std::shared_ptr<MonitorController> createShared(
        OATPP_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, apiObjectMapper)
    ) {
        return std::make_shared<MonitorController>(apiObjectMapper);
    }

    static std::shared_ptr<MonitorController> createSharedWithConfig(
        const oatpp::Object<ConfigDto>& initialConfig
    ) {
        OATPP_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, apiObjectMapper);
        return std::make_shared<MonitorController>(apiObjectMapper, initialConfig);
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
        // Preserve startup configured process watch-list unless explicitly provided.
        if (!configDto->title && m_config) {
            configDto->title = m_config->title;
        }
        if (!configDto->monitoredProcesses && m_config) {
            configDto->monitoredProcesses = m_config->monitoredProcesses;
        }
        applyDefaults(configDto);
        
        m_config = configDto;
        return createDtoResponse(Status::CODE_200, m_config);
    }
};

#include OATPP_CODEGEN_END(ApiController)

#endif // MonitorController_hpp
