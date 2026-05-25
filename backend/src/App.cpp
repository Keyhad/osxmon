#include "AppComponent.hpp"
#include "controller/MonitorController.hpp"
#include "oatpp-swagger/Controller.hpp"
#include "oatpp/network/Server.hpp"

#include <iostream>

void run() {
  // 1. Initialize components
  AppComponent components;

  // 2. Get router component
  OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);

  // 3. Create and register Monitor Controller
  // createShared will automatically resolve its dependencies (apiObjectMapper) from the component registry
  auto monitorController = MonitorController::createShared();
  router->addController(monitorController);

  // 4. Create and register Swagger Controller for OpenAPI spec and Swagger UI
  auto swaggerController = oatpp::swagger::Controller::createShared(monitorController->getEndpoints());
  router->addController(swaggerController);

  // 5. Get Connection Provider and Connection Handler components
  OATPP_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, connectionProvider);
  OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connectionHandler);

  // 6. Run Server
  oatpp::network::Server server(connectionProvider, connectionHandler);
  
  std::cout << "\n==================================================\n";
  std::cout << " osxmon C++ backend server running on port " << connectionProvider->getProperty("port").toString()->c_str() << "\n";
  std::cout << " Swagger UI: http://localhost:8000/swagger/ui\n";
  std::cout << " OpenAPI Spec: http://localhost:8000/api-docs/oas-3.0.0.json\n";
  std::cout << "==================================================\n" << std::endl;

  server.run();
}

int main() {
  oatpp::base::Environment::init();
  run();
  oatpp::base::Environment::destroy();
  return 0;
}
