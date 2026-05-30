cmake_minimum_required(VERSION 3.15)
project(osxmon_backend CXX)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Direct all compiled binaries (including external libraries and their test suites) to _build/test by default
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../_build/test")

include(FetchContent)

# Fetch Oat++ Core with OVERRIDE_FIND_PACKAGE
message(STATUS "Fetching Oat++ Core...")
FetchContent_Declare(
    oatpp
    GIT_REPOSITORY https://github.com/oatpp/oatpp.git
    GIT_TAG        1.3.0-latest
    OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(oatpp)

# Create ALIAS targets for oatpp and oatpp-test so that oatpp-swagger can find them
if(NOT TARGET oatpp::oatpp)
    add_library(oatpp::oatpp ALIAS oatpp)
endif()
if(NOT TARGET oatpp::oatpp-test)
    add_library(oatpp::oatpp-test ALIAS oatpp-test)
endif()

# Fetch Oat++ Swagger
message(STATUS "Fetching Oat++ Swagger...")
FetchContent_Declare(
    oatpp-swagger
    GIT_REPOSITORY https://github.com/oatpp/oatpp-swagger.git
    GIT_TAG        1.3.0-latest
)
FetchContent_MakeAvailable(oatpp-swagger)

# Main Executable
add_executable(osxmon_server 
    src/App.cpp
)

# Override output directory for the main backend server to _build/out
set_target_properties(osxmon_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../_build/out"
)

# Header include paths
target_include_directories(osxmon_server PRIVATE src)

# Link Libraries
target_link_libraries(osxmon_server PRIVATE
    oatpp::oatpp
    oatpp-swagger
    "-framework IOKit"
    "-framework CoreFoundation"
)

# Define Swagger Resources path pointing to the fetched source directory
target_compile_definitions(osxmon_server PRIVATE
    OATPP_SWAGGER_RES_PATH="${oatpp-swagger_SOURCE_DIR}/res"
)
