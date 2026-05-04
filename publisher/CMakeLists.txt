cmake_minimum_required(VERSION 3.20)

project(
    market_data_publisher
    VERSION 1.0.0
    DESCRIPTION "Example market data publisher traffic generator"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MDP_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

file(GLOB_RECURSE PUBLISHER_SOURCE_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)

file(GLOB_RECURSE PUBLISHER_HEADER_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/src/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.hpp"
)

file(GLOB_RECURSE MDP_PUBLIC_API_HEADER_FILES
    CONFIGURE_DEPENDS
    "${MDP_REPO_ROOT}/include/*.h"
    "${MDP_REPO_ROOT}/include/*.hpp"
)

add_executable(market_data_publisher
    ${PUBLISHER_SOURCE_FILES}
    ${PUBLISHER_HEADER_FILES}
    ${MDP_PUBLIC_API_HEADER_FILES}
)

target_include_directories(market_data_publisher
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src"
        "${MDP_REPO_ROOT}/include"
)

target_link_libraries(market_data_publisher PRIVATE mdp_protocol)
if(WIN32)
    target_link_libraries(market_data_publisher PRIVATE Ws2_32 Winhttp)
endif()

if(MSVC)
    target_compile_options(market_data_publisher PRIVATE /W4 /permissive-)
else()
    target_compile_options(market_data_publisher PRIVATE -Wall -Wextra -Wpedantic)
endif()

source_group(
    TREE "${CMAKE_CURRENT_LIST_DIR}/src"
    PREFIX "Publisher Source"
    FILES ${PUBLISHER_SOURCE_FILES} ${PUBLISHER_HEADER_FILES}
)

source_group(
    TREE "${MDP_REPO_ROOT}/include"
    PREFIX "Shared API"
    FILES ${MDP_PUBLIC_API_HEADER_FILES}
)
