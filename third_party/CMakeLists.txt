project(third_party)

macro(append_target_if_exists list_var target_name)
    if(TARGET ${target_name})
        list(APPEND ${list_var} ${target_name})
    endif()
endmacro()

# Platform/backend selection mirrors the top-level project policy.
set(HORIZON_THIRD_PARTY_USE_VULKAN ${USE_VULKAN})
set(HORIZON_THIRD_PARTY_USE_METAL OFF)

if(ANDROID)
    set(HORIZON_THIRD_PARTY_USE_VULKAN ON)
elseif(APPLE)
    set(HORIZON_THIRD_PARTY_USE_VULKAN OFF)
    set(HORIZON_THIRD_PARTY_USE_METAL ON)
endif()

add_library(${PROJECT_NAME} STATIC 3rd_party.cpp)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fms-extensions)
endif()

add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cgltf)
add_library(cgltf::cgltf ALIAS cgltf)

option(BUILD_DXIL_SHADERS "Use DXC Shader Model 6 for shaders" OFF)

option(H3D_MATH_ENABLE_SIMD "Enable SIMD optimizations in h3d_math" ON)
option(H3D_MATH_ENABLE_AVX "Enable AVX path in h3d_math (x86/x64 only, raises CPU baseline)" OFF)

add_library(h3d_math STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/h3d_math/src/h3d_math.c
)
target_include_directories(h3d_math PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/h3d_math/include
)
target_compile_definitions(h3d_math PRIVATE
    H3D_MATH_ENABLE_SIMD=$<BOOL:${H3D_MATH_ENABLE_SIMD}>
    H3D_MATH_ENABLE_AVX=$<BOOL:${H3D_MATH_ENABLE_AVX}>
)

# Accumulate public/private wiring in one place, then apply it once below.
set(HORIZON_THIRD_PARTY_PUBLIC_INCLUDE_DIRS
    include
    cgltf
    spdlog/include
    nlohmann_json/include
)
set(HORIZON_THIRD_PARTY_PUBLIC_LINK_LIBS
    h3d_math
    cgltf::cgltf
    spdlog
)
# Repo submodules that are built as part of this target.
set(SDL_SHARED OFF CACHE BOOL "Build a shared version of the library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build a static version of the library" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL3_test library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build the test directory" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Build the examples directory" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "Enable installation of SDL3" FORCE)
add_subdirectory(sdl3)

list(APPEND HORIZON_THIRD_PARTY_PUBLIC_LINK_LIBS SDL3::SDL3)

add_subdirectory(spdlog)

# Backend-specific third-party dependencies.
if(HORIZON_THIRD_PARTY_USE_VULKAN)
    find_package(Vulkan REQUIRED)

    set(VOLK_PULL_IN_VULKAN ON CACHE BOOL "Link Volk with Vulkan loader" FORCE)
    add_subdirectory(volk)
    if(ANDROID)
        target_compile_definitions(volk PRIVATE RTLD_DEEPBIND=0)
    endif()

    add_subdirectory(VulkanMemoryAllocator)

    target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/spirv-reflect/spirv_reflect.c
    )
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/spirv-reflect/spirv_reflect.c
        PROPERTIES LANGUAGE C
    )

    list(APPEND HORIZON_THIRD_PARTY_PUBLIC_INCLUDE_DIRS
        ${Vulkan_INCLUDE_DIRS}
        VulkanMemoryAllocator/include
        spirv-reflect
        spirv-reflect/include
    )
    list(APPEND HORIZON_THIRD_PARTY_PUBLIC_LINK_LIBS ${Vulkan_LIBRARIES})
    list(APPEND HORIZON_THIRD_PARTY_PRIVATE_LINK_LIBS GPUOpen::VulkanMemoryAllocator)
endif()

if(HORIZON_THIRD_PARTY_USE_METAL)
    list(APPEND HORIZON_THIRD_PARTY_PUBLIC_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/metal-cpp
    )
endif()

if(WIN32)
    list(APPEND HORIZON_THIRD_PARTY_PUBLIC_INCLUDE_DIRS
        dxc_2025_07_14/inc
        directxtk12/Inc
    )
elseif(NOT HORIZON_THIRD_PARTY_USE_METAL)
    list(APPEND HORIZON_THIRD_PARTY_PUBLIC_INCLUDE_DIRS
        dxc_2025_07_14/inc
    )
endif()

# # Optional system SDKs discovered from the host machine.
# option(HORIZON_ENABLE_FBXSDK "Enable Autodesk FBX SDK mesh importer when the SDK is installed" ON)
# set(HORIZON_HAS_FBXSDK OFF)
# if(WIN32 AND HORIZON_ENABLE_FBXSDK)
#     set(_FBXSDK_HINTS)
#     if(DEFINED ENV{FBXSDK_ROOT})
#         list(APPEND _FBXSDK_HINTS "$ENV{FBXSDK_ROOT}")
#     endif()
#     file(GLOB _FBXSDK_AUTODESK_HINTS "C:/Program Files/Autodesk/FBX/FBX SDK/*")
#     list(APPEND _FBXSDK_HINTS ${_FBXSDK_AUTODESK_HINTS})

#     find_path(FBXSDK_INCLUDE_DIR
#         NAMES fbxsdk.h
#         PATHS ${_FBXSDK_HINTS}
#         PATH_SUFFIXES include
#     )

#     set(_FBXSDK_LIB_SUFFIXES
#         lib/x64/release
#         lib/x64/debug
#         lib/vs2022/x64/release
#         lib/vs2022/x64/debug
#         lib/vs2019/x64/release
#         lib/vs2019/x64/debug
#         lib/vs2017/x64/release
#         lib/vs2017/x64/debug
#     )

#     find_library(FBXSDK_LIBRARY_RELEASE
#         NAMES libfbxsdk-md libfbxsdk
#         PATHS ${_FBXSDK_HINTS}
#         PATH_SUFFIXES ${_FBXSDK_LIB_SUFFIXES}
#     )
#     find_library(FBXSDK_LIBRARY_DEBUG
#         NAMES libfbxsdk-mdd libfbxsdk-md libfbxsdk
#         PATHS ${_FBXSDK_HINTS}
#         PATH_SUFFIXES ${_FBXSDK_LIB_SUFFIXES}
#     )

#     if(FBXSDK_INCLUDE_DIR AND (FBXSDK_LIBRARY_RELEASE OR FBXSDK_LIBRARY_DEBUG))
#         add_library(fbxsdk UNKNOWN IMPORTED)
#         set_target_properties(fbxsdk PROPERTIES
#             INTERFACE_INCLUDE_DIRECTORIES "${FBXSDK_INCLUDE_DIR}"
#         )
#         if(FBXSDK_LIBRARY_RELEASE)
#             set_target_properties(fbxsdk PROPERTIES
#                 IMPORTED_LOCATION_RELEASE "${FBXSDK_LIBRARY_RELEASE}"
#             )
#         endif()
#         if(FBXSDK_LIBRARY_DEBUG)
#             set_target_properties(fbxsdk PROPERTIES
#                 IMPORTED_LOCATION_DEBUG "${FBXSDK_LIBRARY_DEBUG}"
#             )
#         endif()
#         set(HORIZON_HAS_FBXSDK ON)
#         message(STATUS "FBX SDK found: ${FBXSDK_INCLUDE_DIR}")
#     else()
#         message(STATUS "FBX SDK not found; FBX import will be disabled")
#     endif()
# endif()

# if(HORIZON_HAS_FBXSDK)
#     list(APPEND HORIZON_THIRD_PARTY_PUBLIC_LINK_LIBS fbxsdk)
#     target_compile_definitions(${PROJECT_NAME} PUBLIC HORIZON_HAS_FBXSDK=1)
# endif()

# Final target wiring.
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${HORIZON_THIRD_PARTY_PUBLIC_INCLUDE_DIRS}
)
target_link_libraries(${PROJECT_NAME}
    PUBLIC ${HORIZON_THIRD_PARTY_PUBLIC_LINK_LIBS}
    PRIVATE ${HORIZON_THIRD_PARTY_PRIVATE_LINK_LIBS}
)

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
            set(DXC_ARCH "arm64")
        else()
            set(DXC_ARCH "x64")
        endif()
    else()
        set(DXC_ARCH "x86")
    endif()

    target_link_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/dxc_2025_07_14/lib/${DXC_ARCH}
    )
    target_link_libraries(${PROJECT_NAME} PUBLIC dxcompiler)
endif()

set(_THIRD_PARTY_FOLDER_TARGETS
    ${PROJECT_NAME}
)
append_target_if_exists(_THIRD_PARTY_FOLDER_TARGETS spdlog)
append_target_if_exists(_THIRD_PARTY_FOLDER_TARGETS volk)
append_target_if_exists(_THIRD_PARTY_FOLDER_TARGETS SDL3-static)
append_target_if_exists(_THIRD_PARTY_FOLDER_TARGETS SDL3-shared)
append_target_if_exists(_THIRD_PARTY_FOLDER_TARGETS VulkanMemoryAllocator)
append_target_if_exists(_THIRD_PARTY_FOLDER_TARGETS uninstall)
append_target_if_exists(_THIRD_PARTY_FOLDER_TARGETS update_mappings)
append_target_if_exists(_THIRD_PARTY_FOLDER_TARGETS zlibstatic)
append_target_if_exists(_THIRD_PARTY_FOLDER_TARGETS fbxsdk)
append_target_if_exists(_THIRD_PARTY_FOLDER_TARGETS cgltf)
append_target_if_exists(_THIRD_PARTY_FOLDER_TARGETS h3d_math)
set_property(TARGET ${_THIRD_PARTY_FOLDER_TARGETS} PROPERTY FOLDER "third_party")
