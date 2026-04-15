cmake_minimum_required(VERSION 3.20)

add_library(mdp_protocol STATIC
    src/MarketDataEventFrame.cpp
)

target_include_directories(mdp_protocol
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/../../include"
)

if(MSVC)
    target_compile_options(mdp_protocol PRIVATE /W4 /permissive-)
else()
    target_compile_options(mdp_protocol PRIVATE -Wall -Wextra -Wpedantic)
endif()
