cmake_minimum_required(VERSION 3.18)

project(Horizon)

include(${CMAKE_SOURCE_DIR}/cmake/compiler_args.cmake)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/horizon)
set(SOLUTION_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Volk: meta-loader for Vulkan (loads all function pointers at runtime, fixes Android/extension link errors)
if(ANDROID AND DEFINED ANDROID_NDK)
    set(VULKAN_HEADERS_INSTALL_DIR "${ANDROID_NDK}/sources/third_party/vulkan/src" CACHE PATH "Vulkan headers for Volk")
endif()
FetchContent_Declare(volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG master
)
set(VOLK_PULL_IN_VULKAN ON)
FetchContent_MakeAvailable(volk)
# Android dlfcn.h does not define RTLD_DEEPBIND (Linux-only)
if(ANDROID)
    target_compile_definitions(volk PRIVATE RTLD_DEEPBIND=0)
endif()

add_subdirectory(third_party)
add_subdirectory(framework)
add_subdirectory(unit_tests)
add_subdirectory(samples)
