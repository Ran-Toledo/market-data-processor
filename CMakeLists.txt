cmake_minimum_required(VERSION 3.20)

project(
    market_data_processor
    VERSION 0.1.0
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
