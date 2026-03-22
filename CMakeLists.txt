cmake_minimum_required(VERSION 3.18)

project(Horizon)

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

# Volk: meta-loader for Vulkan (loads all function pointers at runtime, fixes Android/extension link errors)
if(ANDROID AND DEFINED ANDROID_NDK)
    set(VULKAN_HEADERS_INSTALL_DIR "${ANDROID_NDK}/sources/third_party/vulkan/src" CACHE PATH "Vulkan headers for Volk")
endif()

if(USE_VULKAN AND NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/volk/CMakeLists.txt")
    message(FATAL_ERROR
        "Missing third_party/volk submodule.\n"
        "Run: git submodule update --init --recursive third_party/volk")
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/cgltf/cgltf.h")
    message(FATAL_ERROR
        "Missing third_party/cgltf submodule.\n"
        "Run: git submodule update --init --recursive third_party/cgltf")
endif()

add_subdirectory(third_party)
add_subdirectory(framework)
add_subdirectory(unit_tests)
add_subdirectory(samples)
