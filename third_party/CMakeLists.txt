project(third_party)

set(third_party_folder "third_party")

add_library(${PROJECT_NAME} STATIC 3rd_party.cpp)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fms-extensions )
endif()
# find vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${Vulkan_LIBRARIES})

# Volk from submodule (no FetchContent download during configure)
if(USE_VULKAN)
    set(VOLK_PULL_IN_VULKAN ON CACHE BOOL "Link Volk with Vulkan loader" FORCE)
    add_subdirectory(volk)
    if(TARGET volk AND NOT TARGET volk::volk)
        add_library(volk::volk ALIAS volk)
    endif()
    if(ANDROID)
        # Android dlfcn.h does not define RTLD_DEEPBIND (Linux-only)
        target_compile_definitions(volk PRIVATE RTLD_DEEPBIND=0)
    endif()
    set_property(TARGET volk PROPERTY FOLDER "third_party")
endif()

add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cgltf)
add_library(cgltf::cgltf ALIAS cgltf)

option(HORIZON_ENABLE_FBXSDK "Enable Autodesk FBX SDK mesh importer when the SDK is installed" ON)
set(HORIZON_HAS_FBXSDK OFF)
if(WIN32 AND HORIZON_ENABLE_FBXSDK)
    set(_FBXSDK_HINTS)
    if(DEFINED ENV{FBXSDK_ROOT})
        list(APPEND _FBXSDK_HINTS "$ENV{FBXSDK_ROOT}")
    endif()
    file(GLOB _FBXSDK_AUTODESK_HINTS "C:/Program Files/Autodesk/FBX/FBX SDK/*")
    list(APPEND _FBXSDK_HINTS ${_FBXSDK_AUTODESK_HINTS})

    find_path(FBXSDK_INCLUDE_DIR
        NAMES fbxsdk.h
        PATHS ${_FBXSDK_HINTS}
        PATH_SUFFIXES include
    )

    set(_FBXSDK_LIB_SUFFIXES
        lib/x64/release
        lib/x64/debug
        lib/vs2022/x64/release
        lib/vs2022/x64/debug
        lib/vs2019/x64/release
        lib/vs2019/x64/debug
        lib/vs2017/x64/release
        lib/vs2017/x64/debug
    )

    find_library(FBXSDK_LIBRARY_RELEASE
        NAMES libfbxsdk-md libfbxsdk
        PATHS ${_FBXSDK_HINTS}
        PATH_SUFFIXES ${_FBXSDK_LIB_SUFFIXES}
    )
    find_library(FBXSDK_LIBRARY_DEBUG
        NAMES libfbxsdk-mdd libfbxsdk-md libfbxsdk
        PATHS ${_FBXSDK_HINTS}
        PATH_SUFFIXES ${_FBXSDK_LIB_SUFFIXES}
    )

    if(FBXSDK_INCLUDE_DIR AND (FBXSDK_LIBRARY_RELEASE OR FBXSDK_LIBRARY_DEBUG))
        add_library(fbxsdk UNKNOWN IMPORTED)
        set_target_properties(fbxsdk PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${FBXSDK_INCLUDE_DIR}"
        )
        if(FBXSDK_LIBRARY_RELEASE)
            set_target_properties(fbxsdk PROPERTIES IMPORTED_LOCATION_RELEASE "${FBXSDK_LIBRARY_RELEASE}")
        endif()
        if(FBXSDK_LIBRARY_DEBUG)
            set_target_properties(fbxsdk PROPERTIES IMPORTED_LOCATION_DEBUG "${FBXSDK_LIBRARY_DEBUG}")
        endif()
        set(HORIZON_HAS_FBXSDK ON)
        message(STATUS "FBX SDK found: ${FBXSDK_INCLUDE_DIR}")
    else()
        message(STATUS "FBX SDK not found; FBX import will be disabled")
    endif()
endif()

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

if(H3D_MATH_ENABLE_SIMD AND H3D_MATH_ENABLE_AVX)
    if(MSVC)
        target_compile_options(h3d_math PRIVATE /arch:AVX)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(h3d_math PRIVATE -mavx)
    endif()
endif()

# GLFW is not supported on Android, skip building it
if(NOT ANDROID)
    set(GLFW_LIBRARY_TYPE "STATIC")
    add_subdirectory(glfw3)
endif()

add_subdirectory(spdlog)
add_subdirectory(VulkanMemoryAllocator)

target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/spirv-reflect/spirv_reflect.c)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/spirv-reflect/spirv_reflect.c PROPERTIES LANGUAGE C)

# Link libraries
target_link_libraries(${PROJECT_NAME} PUBLIC h3d_math cgltf::cgltf)
if(ANDROID)
    target_link_libraries(${PROJECT_NAME} PRIVATE spdlog GPUOpen::VulkanMemoryAllocator)
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw spdlog GPUOpen::VulkanMemoryAllocator)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw spdlog GPUOpen::VulkanMemoryAllocator)
endif()

if(HORIZON_HAS_FBXSDK)
    target_link_libraries(${PROJECT_NAME} PUBLIC fbxsdk)
    target_compile_definitions(${PROJECT_NAME} PUBLIC HORIZON_HAS_FBXSDK=1)
endif()

set(_THIRD_PARTY_FOLDER_TARGETS ${PROJECT_NAME} cgltf spdlog VulkanMemoryAllocator h3d_math)
if(TARGET glfw)
    list(APPEND _THIRD_PARTY_FOLDER_TARGETS glfw)
endif()
if(TARGET uninstall)
    list(APPEND _THIRD_PARTY_FOLDER_TARGETS uninstall)
endif()
if(TARGET zlibstatic)
    list(APPEND _THIRD_PARTY_FOLDER_TARGETS zlibstatic)
endif()
if(TARGET update_mappings)
    list(APPEND _THIRD_PARTY_FOLDER_TARGETS update_mappings)
endif()
if(TARGET fbxsdk)
    list(APPEND _THIRD_PARTY_FOLDER_TARGETS fbxsdk)
endif()
set_property(TARGET ${_THIRD_PARTY_FOLDER_TARGETS} PROPERTY FOLDER "third_party")

# Include directories
if(ANDROID)
    target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
        cgltf
        spdlog/include
        VulkanMemoryAllocator/include
        spirv-reflect
        spirv-reflect/include
        nlohmann_json/include
        include
        dxc_2025_07_14/inc
    )
elseif(WIN32)
    target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
        cgltf
        glfw3/include
        spdlog/include
        VulkanMemoryAllocator/include
        spirv-reflect
        spirv-reflect/include
        nlohmann_json/include
        include
        dxc_2025_07_14/inc
        directxtk12/Inc
    )
else()
    target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
        cgltf
        glfw3/include
        spdlog/include
        VulkanMemoryAllocator/include
        spirv-reflect
        spirv-reflect/include
        nlohmann_json/include
        include
        dxc_2025_07_14/inc
    )
endif()

# Add DXC library path and link
if(WIN32)
    # Determine architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
            set(DXC_ARCH "arm64")
        else()
            set(DXC_ARCH "x64")
        endif()
    else()
        set(DXC_ARCH "x86")
    endif()
    
    # Add DXC library directory
    target_link_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/dxc_2025_07_14/lib/${DXC_ARCH})
    
    # Link DXC library
    target_link_libraries(${PROJECT_NAME} PUBLIC dxcompiler)
endif()
