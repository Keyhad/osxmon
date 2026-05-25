#ifndef AppComponent_hpp
#define AppComponent_hpp

#include "oatpp-swagger/Model.hpp"
#include "oatpp-swagger/Resources.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/interceptor/AllowCorsGlobal.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/core/macro/component.hpp"

class AppComponent {
public:

  // Connection Provider on port 8000
  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, serverConnectionProvider)([] {
    return oatpp::network::tcp::server::ConnectionProvider::createShared({"0.0.0.0", 8000, oatpp::network::Address::IP_4});
  }());

  // Router
  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, httpRouter)([] {
    return oatpp::web::server::HttpRouter::createShared();
  }());

  // Connection Handler (HttpConnectionHandler is lightweight and thread-pooled)
  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, httpConnectionHandler)([] {
    OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);
    auto connectionHandler = oatpp::web::server::HttpConnectionHandler::createShared(router);
    
    // Wire up global CORS preflight and headers
    connectionHandler->addRequestInterceptor(std::make_shared<oatpp::web::server::interceptor::AllowOptionsGlobal>());
    connectionHandler->addResponseInterceptor(std::make_shared<oatpp::web::server::interceptor::AllowCorsGlobal>());
    
    return connectionHandler;
  }());

  // JSON Object Mapper
  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, apiObjectMapper)([] {
    return oatpp::parser::json::mapping::ObjectMapper::createShared();
  }());

  // Swagger Documentation configuration
  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::swagger::DocumentInfo>, swaggerDocumentInfo)([] {
    oatpp::swagger::DocumentInfo::Builder builder;
    builder.setTitle("osxmon C++ Server API")
           .setDescription("Lightweight native telemetry service for macOS resource monitoring")
           .setVersion("1.0.0")
           .addServer("http://localhost:8000", "API server on localhost");
    return builder.build();
  }());

  // Swagger UI Static Resources
  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::swagger::Resources>, swaggerResources)([] {
    // OATPP_SWAGGER_RES_PATH is automatically defined by the oatpp-swagger build system.
    // If undefined in some builds, fallback to an empty string (oatpp will search locally).
    #ifdef OATPP_SWAGGER_RES_PATH
      return oatpp::swagger::Resources::loadResources(OATPP_SWAGGER_RES_PATH);
    #else
      return oatpp::swagger::Resources::loadResources("");
    #endif
  }());
};

#endif // AppComponent_hpp
