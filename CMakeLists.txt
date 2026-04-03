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

# Collect all source files
file(GLOB_RECURSE MDP_SOURCE_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Collect all header files
file(GLOB_RECURSE MDP_HEADER_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

# Create executable
add_executable(market_data_processor
    ${MDP_SOURCE_FILES}
    ${MDP_HEADER_FILES}
)

# Include directories
target_include_directories(market_data_processor
    PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Compiler warnings
if(MSVC)
    target_compile_options(market_data_processor PRIVATE /W4 /permissive-)
else()
    target_compile_options(market_data_processor PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Group source files under "src"
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PREFIX "Project Files"
    FILES ${MDP_SOURCE_FILES}
	FILES ${MDP_HEADER_FILES}
)
