#include "AppComponent.hpp"
#include "controller/MonitorController.hpp"
#include "oatpp-swagger/Controller.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/core/base/Environment.hpp"

#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>

void setLogLevel(const std::string& level) {
    auto logger = std::static_pointer_cast<oatpp::base::DefaultLogger>(oatpp::base::Environment::getLogger());
    if (!logger) return;

    // Reset optional priorities
    logger->disablePriority(oatpp::base::DefaultLogger::PRIORITY_V);
    logger->disablePriority(oatpp::base::DefaultLogger::PRIORITY_D);
    logger->disablePriority(oatpp::base::DefaultLogger::PRIORITY_I);
    logger->disablePriority(oatpp::base::DefaultLogger::PRIORITY_W);
    logger->disablePriority(oatpp::base::DefaultLogger::PRIORITY_E);

    if (level == "verbose") {
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_V);
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_D);
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_I);
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_W);
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_E);
    } else if (level == "debug") {
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_D);
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_I);
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_W);
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_E);
    } else if (level == "info") {
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_I);
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_W);
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_E);
    } else if (level == "warning") {
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_W);
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_E);
    } else if (level == "error") {
        logger->enablePriority(oatpp::base::DefaultLogger::PRIORITY_E);
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
  oatpp::base::Environment::init();

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
