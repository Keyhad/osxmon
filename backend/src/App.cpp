#include "AppComponent.hpp"
#include "controller/MonitorController.hpp"
#include "oatpp-swagger/Controller.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/core/base/Environment.hpp"

#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>

#include <chrono>
#include <mutex>

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

void run(const std::string& logLevel) {
  // 1. Initialize components
  AppComponent components;

  // 2. Configure Logging Level
  setLogLevel(logLevel);

  // 3. Get router component
  OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);

  // 4. Create and register Monitor Controller
  auto monitorController = MonitorController::createShared();
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
  for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "-v" || arg == "--verbose") {
          logLevel = "verbose";
      } else if (arg == "--log-level" && i + 1 < argc) {
          logLevel = argv[++i];
          std::transform(logLevel.begin(), logLevel.end(), logLevel.begin(), [](unsigned char c){ return std::tolower(c); });
      }
  }

  run(logLevel);

  oatpp::base::Environment::destroy();
  return 0;
}
