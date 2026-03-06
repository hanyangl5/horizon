project(framework)

# 定义选项来控制是否编译相应的后端
# 可以通过命令行参数设置：-DUSE_VULKAN=ON 或 -DUSE_DX12=OFF
option(USE_VULKAN "Enable Vulkan backend" ON)
option(USE_DX12 "Enable DirectX 12 backend" ON)

if(NOT WIN32 AND USE_DX12)
    message(STATUS "DirectX 12 backend is Windows-only; forcing USE_DX12=OFF on this platform")
    set(USE_DX12 OFF CACHE BOOL "Enable DirectX 12 backend" FORCE)
endif()

# 收集头文件
file(GLOB_RECURSE HEADER_FILES "*.h" "*.hpp")

# 收集除了 rhi/dx12 和 rhi/vulkan 之外的所有源文件
file(GLOB_RECURSE SOURCE_FILES "*.cpp" "*.c")

# 排除 rhi/dx12 和 rhi/vulkan 文件夹中的源文件（稍后根据条件添加）
list(FILTER SOURCE_FILES EXCLUDE REGEX ".*rhi/dx12/.*")
list(FILTER SOURCE_FILES EXCLUDE REGEX ".*rhi/vulkan/.*")

# Exclude GLFW-related files on Android (GLFW is not supported on Android)
if(ANDROID)
    list(FILTER SOURCE_FILES EXCLUDE REGEX ".*core/glfwinput.*")
    list(FILTER HEADER_FILES EXCLUDE REGEX ".*core/glfwinput.*")
endif()

# Include Android sources on Android
if(ANDROID)
    file(GLOB_RECURSE ANDROID_SOURCES "core/android/*.cpp")
    list(APPEND SOURCE_FILES ${ANDROID_SOURCES})
endif()

# 根据变量决定是否添加相应的源文件
if(USE_VULKAN)
    file(GLOB_RECURSE VULKAN_SOURCES "rhi/vulkan/*.cpp" "rhi/vulkan/*.c")
    list(APPEND SOURCE_FILES ${VULKAN_SOURCES})
endif()

if(USE_DX12)
    file(GLOB_RECURSE DX12_SOURCES "rhi/dx12/*.cpp" "rhi/dx12/*.c")
    list(APPEND SOURCE_FILES ${DX12_SOURCES})
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${SOURCE_FILES})

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/framework)

# Add Android NDK include directories (after target is created)
if(ANDROID AND DEFINED ANDROID_NDK)
    # Standard Android NDK includes
    if(EXISTS "${ANDROID_NDK}/sources/android/native_app_glue")
        target_include_directories(${PROJECT_NAME} PUBLIC 
            "${ANDROID_NDK}/sources/android/native_app_glue"
        )
    endif()
    
    # Platform-specific includes
    if(DEFINED ANDROID_PLATFORM AND DEFINED ANDROID_ABI)
        string(REPLACE "android-" "" ANDROID_API_LEVEL "${ANDROID_PLATFORM}")
        # Map ABI to architecture name
        if(ANDROID_ABI STREQUAL "arm64-v8a")
            set(ANDROID_ARCH_NAME "arm64")
        elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
            set(ANDROID_ARCH_NAME "arm")
        elseif(ANDROID_ABI STREQUAL "x86_64")
            set(ANDROID_ARCH_NAME "x86_64")
        elseif(ANDROID_ABI STREQUAL "x86")
            set(ANDROID_ARCH_NAME "x86")
        else()
            set(ANDROID_ARCH_NAME "arm64")  # Default
        endif()
        if(EXISTS "${ANDROID_NDK}/platforms/android-${ANDROID_API_LEVEL}/arch-${ANDROID_ARCH_NAME}/usr/include")
            target_include_directories(${PROJECT_NAME} PUBLIC 
                "${ANDROID_NDK}/platforms/android-${ANDROID_API_LEVEL}/arch-${ANDROID_ARCH_NAME}/usr/include"
            )
        endif()
    endif()
endif()

# 根据变量设置编译定义
if(USE_VULKAN)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_VULKAN)
    # VK_NO_PROTOTYPES: Vulkan headers must not declare function prototypes so Volk can provide them (avoids redefinition on Windows)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)
    # Volk: load Vulkan at runtime (avoids undefined symbols on Android for vkCmdBeginRendering etc.)
    target_link_libraries(${PROJECT_NAME} PRIVATE volk::volk)
    
    # Android-specific configuration
    if(ANDROID)
        # Link Android NDK libraries
        target_link_libraries(${PROJECT_NAME} PUBLIC 
            android      # Android native API
            log          # Android logging
        )
        
        # Add NDK include directories for Vulkan
        if(DEFINED ANDROID_NDK)
            # Vulkan headers are included in NDK
            if(EXISTS "${ANDROID_NDK}/sources/third_party/vulkan/src/include")
                target_include_directories(${PROJECT_NAME} PUBLIC 
                    "${ANDROID_NDK}/sources/third_party/vulkan/src/include"
                )
            endif()
        endif()
    endif()
endif()

if(USE_DX12)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_DX12)
    
    # Configure DXC paths
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
        
        # Set DXC paths
        set(DXC_BIN_DIR "${CMAKE_SOURCE_DIR}/third_party/dxc_2025_07_14/bin/${DXC_ARCH}")
        set(DXC_LIB_DIR "${CMAKE_SOURCE_DIR}/third_party/dxc_2025_07_14/lib/${DXC_ARCH}")
        
        # Convert to native path format (Windows uses backslashes)
        file(TO_NATIVE_PATH "${DXC_BIN_DIR}" DXC_BIN_DIR_NATIVE)
        
        # Escape backslashes for C++ string literal in preprocessor
        string(REPLACE "\\" "\\\\" DXC_BIN_DIR_ESCAPED "${DXC_BIN_DIR_NATIVE}")
        
        # Pass DXC bin directory path to C++ code as a string literal
        target_compile_definitions(${PROJECT_NAME} PRIVATE 
            DXC_BIN_DIR="${DXC_BIN_DIR_ESCAPED}"
        )
        
        # Copy DXC DLLs to output directory for runtime
        if(EXISTS "${DXC_BIN_DIR}/dxcompiler.dll")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DXC_BIN_DIR}/dxcompiler.dll"
                    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/dxcompiler.dll"
                COMMENT "Copying dxcompiler.dll to output directory"
            )
            if(EXISTS "${DXC_BIN_DIR}/dxil.dll")
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${DXC_BIN_DIR}/dxil.dll"
                        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/dxil.dll"
                    COMMENT "Copying dxil.dll to output directory"
                )
            endif()
        endif()
    endif()
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC third_party)

configure_compile_options(${PROJECT_NAME})

# --- Precompiled Header (PCH) ---
target_precompile_headers(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/pch.h
)

# --- Unity Build ---
set_target_properties(${PROJECT_NAME} PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 8
)

# Files with #define ...IMPLEMENTATION must compile alone
# to avoid duplicate symbol / macro leaking issues in unity batches.
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/resource_loader/texture/texture_loader.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)
if(USE_VULKAN)
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/vulkan/rhi_vulkan.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/vulkan/vulkan_swap_chain.cpp
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )
endif()
