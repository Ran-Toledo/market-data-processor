cmake_minimum_required(VERSION 3.20)

project(
    market_data_processor
    VERSION 0.1.0
    DESCRIPTION "Low-latency market data processor in C++"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(market_data_processor
    src/main.cpp
)

target_include_directories(market_data_processor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(market_data_processor PRIVATE cxx_std_17)