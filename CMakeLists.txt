cmake_minimum_required(VERSION 3.20)

project(
    market_data_processor
    VERSION 1.0.0
    DESCRIPTION "Low-latency market data processor in C++"
    LANGUAGES CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Collect source files
file(GLOB_RECURSE MDP_SOURCE_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Collect header files from src
file(GLOB_RECURSE MDP_SRC_HEADER_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

# Collect header files from include
file(GLOB_RECURSE MDP_INCLUDE_HEADER_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

add_subdirectory(lib/protocol)
file(GLOB_RECURSE MDP_PUBLISHER_SOURCE_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/publisher/src/*.cpp"
)
file(GLOB_RECURSE MDP_PUBLISHER_HEADER_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/publisher/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/publisher/src/*.hpp"
)
set(MDP_PUBLISHER_LIBRARY_SOURCE_FILES ${MDP_PUBLISHER_SOURCE_FILES})
list(FILTER MDP_PUBLISHER_LIBRARY_SOURCE_FILES EXCLUDE REGEX ".*/main\\.cpp$")

set(MDP_LIBRARY_SOURCE_FILES ${MDP_SOURCE_FILES})
list(FILTER MDP_LIBRARY_SOURCE_FILES EXCLUDE REGEX ".*/main\\.cpp$")

# Create executable
add_executable(market_data_processor
    ${MDP_SOURCE_FILES}
    ${MDP_SRC_HEADER_FILES}
    ${MDP_INCLUDE_HEADER_FILES}
)

# Include directories
target_include_directories(market_data_processor
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(market_data_processor PRIVATE mdp_protocol)
if(WIN32)
    target_link_libraries(market_data_processor PRIVATE Ws2_32)
endif()

enable_testing()

add_executable(market_data_processor_tests
    tests/main.cpp
    tests/unit/EventProcessorTests.cpp
    tests/unit/EventQueueTests.cpp
    tests/unit/LatencyRecorderTests.cpp
    tests/unit/MarketDataEventFrameTests.cpp
    tests/unit/MetricsCollectorTests.cpp
    tests/unit/IbkrMarketDataSourceTests.cpp
    tests/unit/PortProtocolTests.cpp
    tests/unit/RiskRuleEvaluatorTests.cpp
    tests/unit/SequenceTrackerTests.cpp
    tests/unit/SymbolStateStoreTests.cpp
    tests/unit/SymbolStatsTests.cpp
    ${MDP_LIBRARY_SOURCE_FILES}
    ${MDP_PUBLISHER_LIBRARY_SOURCE_FILES}
    ${MDP_SRC_HEADER_FILES}
    ${MDP_INCLUDE_HEADER_FILES}
    ${MDP_PUBLISHER_HEADER_FILES}
)

target_include_directories(market_data_processor_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/publisher/src"
)

target_link_libraries(market_data_processor_tests PRIVATE mdp_protocol)
if(WIN32)
    target_link_libraries(market_data_processor_tests PRIVATE Ws2_32 Winhttp)
endif()

add_test(NAME unit.sequence_tracker COMMAND market_data_processor_tests unit.sequence_tracker)
add_test(NAME unit.event_processor COMMAND market_data_processor_tests unit.event_processor)
add_test(NAME unit.event_queue COMMAND market_data_processor_tests unit.event_queue)
add_test(NAME unit.latency_recorder COMMAND market_data_processor_tests unit.latency_recorder)
add_test(NAME unit.market_data_event_frame COMMAND market_data_processor_tests unit.market_data_event_frame)
add_test(NAME unit.metrics_collector COMMAND market_data_processor_tests unit.metrics_collector)
add_test(NAME unit.ibkr_market_data_source COMMAND market_data_processor_tests unit.ibkr_market_data_source)
add_test(NAME unit.port_protocol COMMAND market_data_processor_tests unit.port_protocol)
add_test(NAME unit.risk_rule_evaluator COMMAND market_data_processor_tests unit.risk_rule_evaluator)
add_test(NAME unit.symbol_stats COMMAND market_data_processor_tests unit.symbol_stats)
add_test(NAME unit.symbol_state_store COMMAND market_data_processor_tests unit.symbol_state_store)
add_test(
    NAME integration.loopback
    COMMAND powershell
        -NoProfile
        -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/LoopbackIntegrationTest.ps1"
        -ProcessorExe "$<TARGET_FILE:market_data_processor>"
        -PublisherExe "$<TARGET_FILE:market_data_publisher>"
        -WorkDir "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Compiler warnings
if(MSVC)
    target_compile_options(market_data_processor PRIVATE /W4 /permissive-)
else()
    target_compile_options(market_data_processor PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Visual Studio filters for headers under src
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PREFIX "Header Files"
    FILES ${MDP_SRC_HEADER_FILES}
)

# Visual Studio filters for headers under include
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${MDP_INCLUDE_HEADER_FILES}
)

# Visual Studio filters for source files under src
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PREFIX "Source Files"
    FILES ${MDP_SOURCE_FILES}
)

add_subdirectory(publisher)
