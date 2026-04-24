cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json" FORCE)

project(Horizon)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake/)
set(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT DEFINED PROJECT_ROOT OR PROJECT_ROOT STREQUAL "")
    set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH "Horizon runtime project root" FORCE)
endif()

include(Horizon)
