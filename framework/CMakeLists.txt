project(framework)

set(HORIZON_FRAMEWORK_USE_VULKAN OFF)
set(HORIZON_FRAMEWORK_USE_DX12 OFF)
set(HORIZON_FRAMEWORK_USE_METAL OFF)

if(WIN32)
    set(HORIZON_FRAMEWORK_USE_VULKAN ${USE_VULKAN})
    set(HORIZON_FRAMEWORK_USE_DX12 ${USE_DX12})
elseif(ANDROID)
    if(NOT USE_VULKAN)
        message(STATUS "Android only compiles the Vulkan backend; forcing USE_VULKAN=ON in framework")
    endif()
    if(USE_DX12)
        message(STATUS "Android does not compile the DirectX 12 backend; forcing USE_DX12=OFF in framework")
    endif()
    if(USE_METAL)
        message(STATUS "Android does not compile the Metal backend; forcing USE_METAL=OFF in framework")
    endif()
    set(HORIZON_FRAMEWORK_USE_VULKAN ON)
elseif(APPLE)
    if(USE_VULKAN)
        message(STATUS "Apple platforms only compile the Metal backend in framework; forcing USE_VULKAN=OFF")
    endif()
    if(USE_DX12)
        message(STATUS "Apple platforms do not compile the DirectX 12 backend; forcing USE_DX12=OFF in framework")
    endif()
    if(NOT USE_METAL)
        message(STATUS "Apple platforms only compile the Metal backend in framework; forcing USE_METAL=ON")
    endif()
    set(HORIZON_FRAMEWORK_USE_METAL ON)
else()
    set(HORIZON_FRAMEWORK_USE_VULKAN ${USE_VULKAN})
    if(USE_DX12)
        message(STATUS "DirectX 12 is Windows-only; forcing USE_DX12=OFF in framework")
    endif()
    if(USE_METAL)
        message(STATUS "Metal is Apple-only; forcing USE_METAL=OFF in framework")
    endif()
endif()

set(USE_VULKAN ${HORIZON_FRAMEWORK_USE_VULKAN} CACHE BOOL "Enable Vulkan backend" FORCE)
set(USE_DX12 ${HORIZON_FRAMEWORK_USE_DX12} CACHE BOOL "Enable DirectX 12 backend" FORCE)
set(USE_METAL ${HORIZON_FRAMEWORK_USE_METAL} CACHE BOOL "Enable Metal backend" FORCE)

file(GLOB_RECURSE HEADER_FILES "*.h" "*.hpp")
file(GLOB_RECURSE SOURCE_FILES "*.cpp" "*.c" "*.mm")

list(FILTER SOURCE_FILES EXCLUDE REGEX ".*rhi/dx12/.*")
list(FILTER SOURCE_FILES EXCLUDE REGEX ".*rhi/vulkan/.*")
list(FILTER SOURCE_FILES EXCLUDE REGEX ".*rhi/metal/.*")
list(FILTER SOURCE_FILES EXCLUDE REGEX ".*core/glfwwindow_macos\\.(mm|cpp)")

if(ANDROID)
    file(GLOB_RECURSE ANDROID_SOURCES "core/android/*.cpp")
    list(APPEND SOURCE_FILES ${ANDROID_SOURCES})
endif()

if(HORIZON_FRAMEWORK_USE_VULKAN)
    file(GLOB_RECURSE VULKAN_SOURCES "rhi/vulkan/*.cpp" "rhi/vulkan/*.c")
    list(APPEND SOURCE_FILES ${VULKAN_SOURCES})
endif()

if(HORIZON_FRAMEWORK_USE_DX12)
    file(GLOB_RECURSE DX12_SOURCES "rhi/dx12/*.cpp" "rhi/dx12/*.c")
    list(APPEND SOURCE_FILES ${DX12_SOURCES})
endif()

if(HORIZON_FRAMEWORK_USE_METAL)
    file(GLOB_RECURSE METAL_SOURCES "rhi/metal/*.cpp" "rhi/metal/*.mm")
    list(APPEND SOURCE_FILES ${METAL_SOURCES})
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${SOURCE_FILES})

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/framework)

if(ANDROID AND DEFINED ANDROID_NDK)
    if(EXISTS "${ANDROID_NDK}/sources/android/native_app_glue")
        target_include_directories(${PROJECT_NAME} PUBLIC
            "${ANDROID_NDK}/sources/android/native_app_glue"
        )
    endif()

    if(DEFINED ANDROID_PLATFORM AND DEFINED ANDROID_ABI)
        string(REPLACE "android-" "" ANDROID_API_LEVEL "${ANDROID_PLATFORM}")
        if(ANDROID_ABI STREQUAL "arm64-v8a")
            set(ANDROID_ARCH_NAME "arm64")
        elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
            set(ANDROID_ARCH_NAME "arm")
        elseif(ANDROID_ABI STREQUAL "x86_64")
            set(ANDROID_ARCH_NAME "x86_64")
        elseif(ANDROID_ABI STREQUAL "x86")
            set(ANDROID_ARCH_NAME "x86")
        else()
            set(ANDROID_ARCH_NAME "arm64")
        endif()

        if(EXISTS "${ANDROID_NDK}/platforms/android-${ANDROID_API_LEVEL}/arch-${ANDROID_ARCH_NAME}/usr/include")
            target_include_directories(${PROJECT_NAME} PUBLIC
                "${ANDROID_NDK}/platforms/android-${ANDROID_API_LEVEL}/arch-${ANDROID_ARCH_NAME}/usr/include"
            )
        endif()
    endif()
endif()

if(HORIZON_FRAMEWORK_USE_VULKAN)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_VULKAN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)
    target_link_libraries(${PROJECT_NAME} PRIVATE volk::volk)

    if(ANDROID)
        target_link_libraries(${PROJECT_NAME} PUBLIC
            android
            log
        )

        if(DEFINED ANDROID_NDK)
            if(EXISTS "${ANDROID_NDK}/sources/third_party/vulkan/src/include")
                target_include_directories(${PROJECT_NAME} PUBLIC
                    "${ANDROID_NDK}/sources/third_party/vulkan/src/include"
                )
            endif()
        endif()
    endif()
endif()

if(HORIZON_FRAMEWORK_USE_DX12)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_DX12)

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

        set(DXC_BIN_DIR "${CMAKE_SOURCE_DIR}/third_party/dxc_2025_07_14/bin/${DXC_ARCH}")
        set(DXC_LIB_DIR "${CMAKE_SOURCE_DIR}/third_party/dxc_2025_07_14/lib/${DXC_ARCH}")

        file(TO_NATIVE_PATH "${DXC_BIN_DIR}" DXC_BIN_DIR_NATIVE)
        string(REPLACE "\\" "\\\\" DXC_BIN_DIR_ESCAPED "${DXC_BIN_DIR_NATIVE}")

        target_compile_definitions(${PROJECT_NAME} PRIVATE
            DXC_BIN_DIR="${DXC_BIN_DIR_ESCAPED}"
        )

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

if(HORIZON_FRAMEWORK_USE_METAL)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_METAL)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC third_party)

if(HORIZON_FRAMEWORK_USE_METAL)
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${APPKIT_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
    )
endif()

configure_compile_options(${PROJECT_NAME})

target_precompile_headers(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/pch.h
)

function(horizon_assign_unity_group group_name)
    set(group_sources ${ARGN})
    if(group_sources)
        set_source_files_properties(${group_sources} PROPERTIES UNITY_GROUP ${group_name})
    endif()
endfunction()

set_target_properties(${PROJECT_NAME} PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_MODE GROUP
)

set(_FRAMEWORK_CORE_SOURCES ${SOURCE_FILES})
list(FILTER _FRAMEWORK_CORE_SOURCES INCLUDE REGEX ".*/core/.*\\.(cpp|c|mm)$")
list(FILTER _FRAMEWORK_CORE_SOURCES EXCLUDE REGEX ".*/core/android/.*")

set(_FRAMEWORK_APP_SOURCES ${SOURCE_FILES})
list(FILTER _FRAMEWORK_APP_SOURCES INCLUDE REGEX ".*/app_framework/.*\\.(cpp|c|mm)$")

set(_FRAMEWORK_RENDER_GRAPH_SOURCES ${SOURCE_FILES})
list(FILTER _FRAMEWORK_RENDER_GRAPH_SOURCES INCLUDE REGEX ".*/render_graph/.*\\.(cpp|c|mm)$")

set(_FRAMEWORK_RESOURCE_SOURCES ${SOURCE_FILES})
list(FILTER _FRAMEWORK_RESOURCE_SOURCES INCLUDE REGEX ".*/resource/.*\\.(cpp|c|mm)$")
list(FILTER _FRAMEWORK_RESOURCE_SOURCES EXCLUDE REGEX ".*/stb_image_impl\\.cpp$")
list(FILTER _FRAMEWORK_RESOURCE_SOURCES EXCLUDE REGEX ".*/cgltf_impl\\.cpp$")

set(_FRAMEWORK_SCENE_SOURCES ${SOURCE_FILES})
list(FILTER _FRAMEWORK_SCENE_SOURCES INCLUDE REGEX ".*/scene/.*\\.(cpp|c|mm)$")

set(_FRAMEWORK_RHI_COMMON_SOURCES ${SOURCE_FILES})
list(FILTER _FRAMEWORK_RHI_COMMON_SOURCES INCLUDE REGEX ".*/rhi/[^/]+\\.(cpp|c|mm)$")

horizon_assign_unity_group("framework-core" ${_FRAMEWORK_CORE_SOURCES})
horizon_assign_unity_group("framework-app" ${_FRAMEWORK_APP_SOURCES})
horizon_assign_unity_group("framework-render-graph" ${_FRAMEWORK_RENDER_GRAPH_SOURCES})
horizon_assign_unity_group("framework-resource" ${_FRAMEWORK_RESOURCE_SOURCES})
horizon_assign_unity_group("framework-scene" ${_FRAMEWORK_SCENE_SOURCES})
horizon_assign_unity_group("framework-rhi-common" ${_FRAMEWORK_RHI_COMMON_SOURCES})

if(HORIZON_FRAMEWORK_USE_VULKAN)
    set(_FRAMEWORK_RHI_VULKAN_SOURCES ${SOURCE_FILES})
    list(FILTER _FRAMEWORK_RHI_VULKAN_SOURCES INCLUDE REGEX ".*/rhi/vulkan/.*\\.(cpp|c)$")
    list(FILTER _FRAMEWORK_RHI_VULKAN_SOURCES EXCLUDE REGEX ".*/vma_impl\\.cpp$")
    list(FILTER _FRAMEWORK_RHI_VULKAN_SOURCES EXCLUDE REGEX ".*/vulkan_swap_chain\\.cpp$")
    horizon_assign_unity_group("framework-rhi-vulkan" ${_FRAMEWORK_RHI_VULKAN_SOURCES})
endif()

if(HORIZON_FRAMEWORK_USE_DX12)
    set(_FRAMEWORK_RHI_DX12_SHADER_SOURCES ${SOURCE_FILES})
    list(FILTER _FRAMEWORK_RHI_DX12_SHADER_SOURCES INCLUDE REGEX ".*/rhi/dx12/(dx12_shader|dx12_shader_compiler)\\.cpp$")

    set(_FRAMEWORK_RHI_DX12_RUNTIME_SOURCES ${SOURCE_FILES})
    list(FILTER _FRAMEWORK_RHI_DX12_RUNTIME_SOURCES INCLUDE REGEX ".*/rhi/dx12/.*\\.(cpp|c)$")
    list(FILTER _FRAMEWORK_RHI_DX12_RUNTIME_SOURCES EXCLUDE REGEX ".*/rhi/dx12/(dx12_shader|dx12_shader_compiler)\\.cpp$")

    horizon_assign_unity_group("framework-rhi-dx12-shaders" ${_FRAMEWORK_RHI_DX12_SHADER_SOURCES})
    horizon_assign_unity_group("framework-rhi-dx12-runtime" ${_FRAMEWORK_RHI_DX12_RUNTIME_SOURCES})
endif()

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/resource_loader/texture/stb_image_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/resources/mesh/cgltf_impl.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)

if(HORIZON_FRAMEWORK_USE_VULKAN)
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/vulkan/vma_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/vulkan/vulkan_swap_chain.cpp
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )
endif()

if(HORIZON_FRAMEWORK_USE_METAL)
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/metal/metal_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/metal/metal_buffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/metal/metal_texture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/metal/metal_render_target.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/metal/metal_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/metal/metal_sampler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/metal/metal_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/metal/metal_swap_chain.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/metal/metal_command_list.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/metal/metal_semaphore.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/metal/rhi_metal.cpp
        PROPERTIES
            SKIP_UNITY_BUILD_INCLUSION ON
            SKIP_PRECOMPILE_HEADERS ON
    )
endif()
