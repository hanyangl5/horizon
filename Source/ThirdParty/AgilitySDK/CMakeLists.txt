project(DirectXAgilitySDK)

if(NOT DEFINED HORIZON_AGILITYSDK_VERSION OR NOT DEFINED HORIZON_AGILITYSDK_SDK_VERSION)
    message(FATAL_ERROR "Agility SDK version variables are missing. Include CMake/GpuDeps.cmake before adding AgilitySDK.")
endif()

set(AGILITYSDK_PACKAGE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/microsoft.direct3d.d3d12.${HORIZON_AGILITYSDK_VERSION}"
)

if(NOT EXISTS "${AGILITYSDK_PACKAGE_DIR}/include/d3d12.h")
    message(FATAL_ERROR
        "D3D12 Agility SDK ${HORIZON_AGILITYSDK_VERSION} is missing. "
        "Run `python Scripts/UpdateGpuDeps.py --deps agility_sdk` from the repository root."
    )
endif()

add_library(agilitysdk INTERFACE)
target_include_directories(agilitysdk INTERFACE
    "${AGILITYSDK_PACKAGE_DIR}/include"
    "${AGILITYSDK_PACKAGE_DIR}/include/d3dx12"
)
target_compile_definitions(agilitysdk INTERFACE
    D3D12_AGILITY_SDK=1
    D3D12_AGILITY_SDK_VERSION=${HORIZON_AGILITYSDK_SDK_VERSION}
)

if(MSVC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_link_options(agilitysdk INTERFACE /include:D3D12SDKVersion /include:D3D12SDKPath)
    else()
        target_link_options(agilitysdk INTERFACE /include:_D3D12SDKVersion /include:_D3D12SDKPath)
    endif()
endif()

set(HORIZON_AGILITYSDK_RUNTIME_DLLS
    "${AGILITYSDK_PACKAGE_DIR}/bin/x64/D3D12Core.dll"
    "${AGILITYSDK_PACKAGE_DIR}/bin/x64/d3d12SDKLayers.dll"
    CACHE INTERNAL "D3D12 Agility SDK runtime DLLs"
)

set(AGILITYSDK_DIR "${AGILITYSDK_PACKAGE_DIR}" CACHE INTERNAL "")
set(AGILITYSDK_D3D12CORE_DLL "${AGILITYSDK_PACKAGE_DIR}/bin/x64/D3D12Core.dll" CACHE INTERNAL "")
set(AGILITYSDK_D3D12SDKLAYERS_DLL "${AGILITYSDK_PACKAGE_DIR}/bin/x64/d3d12SDKLayers.dll" CACHE INTERNAL "")
