#include "AppComponent.hpp"
#include "controller/MonitorController.hpp"
#include "oatpp-swagger/Controller.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/core/base/Environment.hpp"

#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <chrono>
#include <mutex>

namespace {

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

std::string stripQuotes(const std::string& input) {
    if (input.size() >= 2) {
        if ((input.front() == '\'' && input.back() == '\'') ||
                (input.front() == '"' && input.back() == '"')) {
            return input.substr(1, input.size() - 2);
        }
    }
    return input;
}

bool parseBool(const std::string& value, bool& out) {
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "true") {
        out = true;
        return true;
    }
    if (lowered == "false") {
        out = false;
        return true;
    }
    return false;
}

void applyYamlConfigEntry(const oatpp::Object<ConfigDto>& config, const std::string& key, const std::string& rawValue) {
    const std::string value = trim(rawValue);

    if (key == "pollingIntervalMs") {
        config->pollingIntervalMs = std::stoi(value);
        return;
    }
    if (key == "maxProcesses") {
        config->maxProcesses = std::stoi(value);
        return;
    }

    bool parsedBool = false;
    bool boolValue = false;
    if (parseBool(value, boolValue)) {
        parsedBool = true;
    }
    if (!parsedBool) {
        return;
    }

    if (key == "enableCpu") config->enableCpu = boolValue;
    else if (key == "enableMemory") config->enableMemory = boolValue;
    else if (key == "enableDisk") config->enableDisk = boolValue;
    else if (key == "enableNetwork") config->enableNetwork = boolValue;
    else if (key == "enableProcesses") config->enableProcesses = boolValue;
}

oatpp::Object<ConfigDto> loadConfigFromYaml(const std::string& configPath) {
    std::ifstream input(configPath);
    if (!input.is_open()) {
        throw std::runtime_error("Unable to open config file: " + configPath);
    }

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

    enum class Section {
        None,
        Config,
        MonitoredProcesses
    };
    Section section = Section::None;

    std::string line;
    while (std::getline(input, line)) {
        const std::size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        if (trim(line).empty()) {
            continue;
        }

        const bool topLevel = line.find_first_not_of(" \t") == 0;
        const std::string trimmedLine = trim(line);

        if (topLevel) {
            if (trimmedLine.rfind("title:", 0) == 0) {
                std::string value = trim(trimmedLine.substr(std::string("title:").size()));
                value = stripQuotes(value);
                if (!value.empty()) {
                    config->title = value.c_str();
                }
                section = Section::None;
            } else if (trimmedLine == "config:") {
                section = Section::Config;
            } else if (trimmedLine == "monitoredProcesses:") {
                section = Section::MonitoredProcesses;
            } else {
                section = Section::None;
            }
            continue;
        }

        if (section == Section::Config) {
            const std::size_t sep = trimmedLine.find(':');
            if (sep == std::string::npos) {
                continue;
            }
            const std::string key = trim(trimmedLine.substr(0, sep));
            const std::string value = trim(trimmedLine.substr(sep + 1));
            if (!key.empty() && !value.empty()) {
                applyYamlConfigEntry(config, key, value);
            }
        } else if (section == Section::MonitoredProcesses) {
            if (!trimmedLine.empty() && trimmedLine[0] == '-') {
                std::string processName = trim(trimmedLine.substr(1));
                processName = stripQuotes(processName);
                if (!processName.empty()) {
                    config->monitoredProcesses->push_back(processName.c_str());
                }
            }
        }
    }

    return config;
}

} // namespace

class AppLogger : public oatpp::base::Logger {
private:
    std::mutex m_lock;
    uint32_t m_logMask;
public:
    AppLogger() : m_logMask((1 << PRIORITY_V) | (1 << PRIORITY_D) | (1 << PRIORITY_I) | (1 << PRIORITY_W) | (1 << PRIORITY_E)) {}

    void enablePriority(v_uint32 priority) {
        if (priority <= PRIORITY_E) m_logMask |= (1U << priority);
    }

    void disablePriority(v_uint32 priority) {
        if (priority <= PRIORITY_E) m_logMask &= ~(1U << priority);
    }

    bool isLogPriorityEnabled(v_uint32 priority) override {
        if (priority > PRIORITY_E) return true;
        return (m_logMask & (1U << priority));
    }

    void log(v_uint32 priority, const std::string& tag, const std::string& message) override {
        std::lock_guard<std::mutex> lock(m_lock);
        
        auto time = std::chrono::system_clock::now();
        time_t seconds = std::chrono::system_clock::to_time_t(time);
        tm now;
        localtime_r(&seconds, &now);
        char timeBuffer[50];
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &now);

        auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count();

        const char* prioStr = " ? ";
        switch (priority) {
            case PRIORITY_V: prioStr = " V "; break;
            case PRIORITY_D: prioStr = " D "; break;
            case PRIORITY_I: prioStr = " I "; break;
            case PRIORITY_W: prioStr = " W "; break;
            case PRIORITY_E: prioStr = " E "; break;
        }

        std::cout << prioStr << "|" << timeBuffer << " " << ticks << "| " << tag << ":" << message << std::endl;
    }
};

void setLogLevel(const std::string& level) {
    auto logger = std::static_pointer_cast<AppLogger>(oatpp::base::Environment::getLogger());
    if (!logger) return;

    // Reset optional priorities
    logger->disablePriority(AppLogger::PRIORITY_V);
    logger->disablePriority(AppLogger::PRIORITY_D);
    logger->disablePriority(AppLogger::PRIORITY_I);
    logger->disablePriority(AppLogger::PRIORITY_W);
    logger->disablePriority(AppLogger::PRIORITY_E);

    if (level == "verbose") {
        logger->enablePriority(AppLogger::PRIORITY_V);
        logger->enablePriority(AppLogger::PRIORITY_D);
        logger->enablePriority(AppLogger::PRIORITY_I);
        logger->enablePriority(AppLogger::PRIORITY_W);
        logger->enablePriority(AppLogger::PRIORITY_E);
    } else if (level == "debug") {
        logger->enablePriority(AppLogger::PRIORITY_D);
        logger->enablePriority(AppLogger::PRIORITY_I);
        logger->enablePriority(AppLogger::PRIORITY_W);
        logger->enablePriority(AppLogger::PRIORITY_E);
    } else if (level == "info") {
        logger->enablePriority(AppLogger::PRIORITY_I);
        logger->enablePriority(AppLogger::PRIORITY_W);
        logger->enablePriority(AppLogger::PRIORITY_E);
    } else if (level == "warning") {
        logger->enablePriority(AppLogger::PRIORITY_W);
        logger->enablePriority(AppLogger::PRIORITY_E);
    } else if (level == "error") {
        logger->enablePriority(AppLogger::PRIORITY_E);
    }
}

void run(const std::string& logLevel, const oatpp::Object<ConfigDto>& initialConfig = nullptr) {
  // 1. Initialize components
  AppComponent components;

  // 2. Configure Logging Level
  setLogLevel(logLevel);

  // 3. Get router component
  OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);

  // 4. Create and register Monitor Controller
    auto monitorController = initialConfig
            ? MonitorController::createSharedWithConfig(initialConfig)
            : MonitorController::createShared();
  router->addController(monitorController);

  // 5. Create and register Swagger Controller
  auto swaggerController = oatpp::swagger::Controller::createShared(monitorController->getEndpoints());
  router->addController(swaggerController);

  // 6. Get Connection Provider and Connection Handler
  OATPP_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, connectionProvider);
  OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connectionHandler);

  // 7. Run Server
  oatpp::network::Server server(connectionProvider, connectionHandler);
  
  OATPP_LOGI("Server", "\n==================================================");
  OATPP_LOGI("Server", "osxmon C++ backend server running on port %s", connectionProvider->getProperty("port").toString()->c_str());
  OATPP_LOGI("Server", "Swagger UI: http://localhost:8000/swagger/ui");
  OATPP_LOGI("Server", "OpenAPI Spec: http://localhost:8000/api-docs/oas-3.0.0.json");
  OATPP_LOGI("Server", "Active Log Level: %s", logLevel.c_str());
  OATPP_LOGI("Server", "==================================================");

  server.run();
}

int main(int argc, char* argv[]) {
  // Force line-buffering so logs appear in the file immediately when daemonized
  setvbuf(stdout, nullptr, _IOLBF, 0);
  setvbuf(stderr, nullptr, _IOLBF, 0);

  oatpp::base::Environment::init(std::make_shared<AppLogger>());

  std::string logLevel = "info";
  std::string configPath;
  for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "-v" || arg == "--verbose") {
          logLevel = "verbose";
      } else if (arg == "--log-level" && i + 1 < argc) {
          logLevel = argv[++i];
          std::transform(logLevel.begin(), logLevel.end(), logLevel.begin(), [](unsigned char c){ return std::tolower(c); });
      } else if (arg == "--config" && i + 1 < argc) {
          configPath = argv[++i];
      }
  }

  oatpp::Object<ConfigDto> initialConfig;
  if (!configPath.empty()) {
      try {
          initialConfig = loadConfigFromYaml(configPath);
          OATPP_LOGI("Server", "Loaded startup config from %s", configPath.c_str());
      } catch (const std::exception& ex) {
          OATPP_LOGE("Server", "Failed to load --config file: %s", ex.what());
          oatpp::base::Environment::destroy();
          return 1;
      }
  }

  run(logLevel, initialConfig);

  oatpp::base::Environment::destroy();
  return 0;
}
