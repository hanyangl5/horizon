cmake_minimum_required(VERSION 3.18)

project(Horizon LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/compiler_args.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/pgo.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/lto.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/horizon)
set(SOLUTION_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_VULKAN "Enable Vulkan backend" ON)
option(USE_DX12 "Enable DirectX 12 backend" ON)
option(USE_METAL "Enable Metal backend" OFF)

add_subdirectory(third_party)
add_subdirectory(framework)
add_subdirectory(unit_tests)
add_subdirectory(samples)
